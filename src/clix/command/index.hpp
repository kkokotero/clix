#pragma once

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../detail/strings.hpp"
#include "../exceptions/command_error/index.hpp"
#include "../parser/value/index.hpp"
#include "constants.hpp"
#include "types.hpp"

namespace clix {

namespace detail {

inline std::vector<CompletionSuggestion> path_completion_suggestions(std::string_view prefix) {
    std::vector<CompletionSuggestion> suggestions;

    const auto text = std::string(prefix);
    std::filesystem::path prefix_path(text.empty() ? "." : text);
    auto base = prefix_path.parent_path();
    auto fragment = prefix_path.filename().string();

    if (detail::ends_with(text, "/") || detail::ends_with(text, "\\")) {
        base = prefix_path;
        fragment.clear();
    }

    if (base.empty()) {
        base = ".";
    }

    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(base, error)) {
        if (error) {
            break;
        }

        const auto name = entry.path().filename().string();
        if (!fragment.empty() && !detail::starts_with(name, fragment)) {
            continue;
        }

        auto value = (base == "." ? std::filesystem::path(name) : base / name).string();
        if (entry.is_directory(error) && !detail::ends_with(value, "/")) {
            value += '/';
        }

        suggestions.push_back({value, entry.is_directory(error) ? "directory" : "path"});
    }

    return suggestions;
}

inline ValueKind completion_base_kind(ValueKind kind) {
    switch (kind) {
        case ValueKind::boolean_array:
            return ValueKind::boolean;
        case ValueKind::string_array:
            return ValueKind::string;
        case ValueKind::number_array:
            return ValueKind::number;
        case ValueKind::path_array:
            return ValueKind::path;
        case ValueKind::url_array:
            return ValueKind::url;
        case ValueKind::time_array:
            return ValueKind::time;
        case ValueKind::size_array:
            return ValueKind::size;
        default:
            return kind;
    }
}

inline bool is_array_kind(ValueKind kind) {
    return completion_base_kind(kind) != kind;
}

inline std::pair<std::string, std::string> split_completion_prefix(ValueKind kind, std::string_view prefix) {
    if (!is_array_kind(kind)) {
        return {"", std::string(prefix)};
    }

    const auto delimiter = prefix.rfind(',');
    if (delimiter == std::string_view::npos) {
        return {"", detail::trim_copy(prefix)};
    }

    auto head = std::string(prefix.substr(0, delimiter + 1));
    auto tail = detail::trim_copy(prefix.substr(delimiter + 1));

    if (!head.empty() && !detail::ends_with(head, " ")) {
        head += ' ';
    }

    return {head, tail};
}

inline std::vector<CompletionSuggestion> resolve_value_completion(
    ValueKind kind,
    const std::vector<std::string>& choices,
    const std::vector<CompletionSuggestion>& completion_values,
    const CompletionProvider& completion_provider,
    std::string_view prefix) {
    const auto [head, tail] = split_completion_prefix(kind, prefix);
    std::vector<CompletionSuggestion> suggestions;

    if (completion_provider) {
        suggestions = completion_provider(tail);
    }

    for (const auto& choice : choices) {
        suggestions.push_back({choice, "choice"});
    }

    for (const auto& suggestion : completion_values) {
        suggestions.push_back(suggestion);
    }

    switch (completion_base_kind(kind)) {
        case ValueKind::boolean:
            suggestions.push_back({"true", "boolean"});
            suggestions.push_back({"false", "boolean"});
            break;
        case ValueKind::path: {
            const auto path_suggestions = path_completion_suggestions(tail);
            suggestions.insert(suggestions.end(), path_suggestions.begin(), path_suggestions.end());
            break;
        }
        default:
            break;
    }

    filter_completion_suggestions(suggestions, tail);
    for (auto& suggestion : suggestions) {
        suggestion.value = head + suggestion.value;
    }
    return suggestions;
}

inline std::string default_value_kind_label(const ArgumentConfig& config) {
    if (!config.value_label.empty()) {
        return config.value_label;
    }
    if (!config.custom_type_name.empty()) {
        return config.custom_type_name;
    }
    return to_string(config.kind);
}

inline std::string default_value_kind_label(const OptionConfig& config) {
    if (!config.value_label.empty()) {
        return config.value_label;
    }
    if (!config.custom_type_name.empty()) {
        return config.custom_type_name;
    }
    return to_string(config.kind);
}

inline std::string prefixed_option_name(std::string_view name) {
    return "--" + std::string(name);
}

inline std::string prefixed_alias_name(std::string_view name) {
    return "-" + std::string(name);
}

inline JsonArray json_array_from_strings(const std::vector<std::string>& values) {
    JsonArray array;
    array.reserve(values.size());
    for (const auto& value : values) {
        array.emplace_back(value);
    }
    return array;
}

inline JsonArray json_array_from_validator_names(const std::vector<ValidatorSpec>& validators) {
    JsonArray array;
    for (const auto& validator : validators) {
        if (!validator.name.empty()) {
            array.emplace_back(validator.name);
        }
    }
    return array;
}

inline JsonArray json_array_from_completion_values(const std::vector<CompletionSuggestion>& suggestions) {
    JsonArray array;
    array.reserve(suggestions.size());
    for (const auto& suggestion : suggestions) {
        JsonObject entry;
        entry.emplace("value", suggestion.value);
        if (!suggestion.description.empty()) {
            entry.emplace("description", suggestion.description);
        }
        array.emplace_back(std::move(entry));
    }
    return array;
}

inline std::string markdown_escape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        switch (character) {
            case '|':
                result += "\\|";
                break;
            case '\n':
                result += "<br>";
                break;
            default:
                result.push_back(character);
                break;
        }
    }
    return result;
}

inline void append_markdown_bullet(std::string& out, std::string_view label, std::string_view value) {
    if (value.empty()) {
        return;
    }
    out += "- **";
    out += std::string(label);
    out += "**: ";
    out += std::string(value);
    out += '\n';
}

template <typename Parser>
using parser_result_t = std::decay_t<std::invoke_result_t<Parser, std::string_view>>;

template <typename T, typename Parser>
CustomValueParser make_typed_parser(Parser parser, std::string type_name) {
    return [parser = std::move(parser), type_name = std::move(type_name)](std::string_view raw) -> CliValue {
        return make_typed_value(static_cast<T>(std::invoke(parser, raw)), type_name);
    };
}

template <typename T, typename Parser, typename Formatter>
CustomValueParser make_typed_parser(Parser parser, Formatter formatter, std::string type_name) {
    return [parser = std::move(parser),
            formatter = std::move(formatter),
            type_name = std::move(type_name)](std::string_view raw) -> CliValue {
        return make_typed_value(static_cast<T>(std::invoke(parser, raw)), type_name, formatter);
    };
}

}  // namespace detail

/**
 * Modular command builder used by both regular commands and the root CLI.
 */
class Command {
public:
    explicit Command(std::string name = {})
        : name_(std::move(name)) {
        option_groups_.emplace("Options", OptionGroupDefinition{"Options", ""});
    }

    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;
    Command(Command&&) = delete;
    Command& operator=(Command&&) = delete;

    virtual ~Command() = default;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const std::string& description_text() const noexcept { return description_; }
    [[nodiscard]] const std::vector<std::string>& aliases() const noexcept { return aliases_; }
    [[nodiscard]] const std::string& deprecated_message() const noexcept { return deprecated_message_; }
    [[nodiscard]] const std::unordered_map<std::string, std::string>& deprecated_alias_messages() const noexcept {
        return deprecated_alias_messages_;
    }
    [[nodiscard]] bool is_deprecated() const noexcept { return !deprecated_message_.empty(); }
    [[nodiscard]] const std::vector<std::pair<std::string, ArgumentConfig>>& arguments_definitions() const noexcept {
        return arguments_;
    }
    [[nodiscard]] const std::vector<std::pair<std::string, OptionConfig>>& options_definitions() const noexcept {
        return options_;
    }
    [[nodiscard]] const std::map<std::string, std::unique_ptr<Command>>& commands() const noexcept { return commands_; }
    [[nodiscard]] const std::map<std::string, OptionGroupDefinition>& option_groups() const noexcept {
        return option_groups_;
    }
    [[nodiscard]] const CommandHandler& handler() const noexcept { return handler_; }
    [[nodiscard]] const Command* parent() const noexcept { return parent_; }

    [[nodiscard]] std::vector<std::string> command_path() const {
        std::vector<std::string> path;
        auto current = this;

        while (current != nullptr) {
            if (!current->name_.empty()) {
                path.push_back(current->name_);
            }
            current = current->parent_;
        }

        std::reverse(path.begin(), path.end());
        if (path.empty()) {
            path.push_back("<command>");
        }

        return path;
    }

    [[nodiscard]] std::string command_path_string() const {
        return detail::join_strings(command_path(), " ");
    }

    Command& alias(std::string alias_name) {
        if (alias_name.empty()) {
            throw CommandError("Alias cannot be empty");
        }

        validate_command_alias_available(alias_name);

        if (std::find(aliases_.begin(), aliases_.end(), alias_name) != aliases_.end()) {
            throw CommandError(
                "Command alias is already defined",
                CommandErrorOptions{"", "", "", {}, {alias_name}, {}, "Use unique aliases per command."});
        }

        aliases_.push_back(std::move(alias_name));
        return *this;
    }

    Command& deprecated_alias(std::string alias_name,
                              std::string message = "This command alias is deprecated.") {
        alias(std::move(alias_name));
        deprecated_alias_messages_[aliases_.back()] = std::move(message);
        return *this;
    }

    Command& description(std::string description_text) {
        description_ = std::move(description_text);
        return *this;
    }

    Command& deprecated(std::string message = "This command is deprecated.") {
        deprecated_message_ = std::move(message);
        return *this;
    }

    Command& allow_unknown_options(bool value = true) {
        allow_unknown_options_ = value;
        return *this;
    }

    Command& allow_extra_arguments(bool value = true) {
        allow_extra_arguments_ = value;
        return *this;
    }

    Command& allow_passthrough(bool value = true) {
        allow_unknown_options_ = value;
        allow_extra_arguments_ = value;
        return *this;
    }

    template <typename Bundle>
    Command& use(Bundle&& bundle) {
        std::forward<Bundle>(bundle)(*this);
        return *this;
    }

    [[nodiscard]] bool allows_unknown_options() const noexcept { return allow_unknown_options_; }
    [[nodiscard]] bool allows_extra_arguments() const noexcept { return allow_extra_arguments_; }
    [[nodiscard]] bool allows_passthrough() const noexcept {
        return allow_unknown_options_ && allow_extra_arguments_;
    }

    [[nodiscard]] JsonObject schema() const {
        JsonObject result;
        result.emplace("name", name_);
        result.emplace("description", description_);
        result.emplace("usage", usage());
        result.emplace("path", JsonValue(detail::json_array_from_strings(command_path())));
        if (!aliases_.empty()) {
            result.emplace("aliases", JsonValue(detail::json_array_from_strings(aliases_)));
        }
        if (!deprecated_message_.empty()) {
            result.emplace("deprecated", true);
            result.emplace("deprecated_message", deprecated_message_);
        }
        if (!deprecated_alias_messages_.empty()) {
            JsonObject deprecated_aliases;
            for (const auto& [alias_name, message] : deprecated_alias_messages_) {
                deprecated_aliases.emplace(alias_name, message);
            }
            result.emplace("deprecated_aliases", std::move(deprecated_aliases));
        }
        if (allows_passthrough()) {
            result.emplace("passthrough", true);
        } else {
            if (allow_unknown_options_) {
                result.emplace("allow_unknown_options", true);
            }
            if (allow_extra_arguments_) {
                result.emplace("allow_extra_arguments", true);
            }
        }

        JsonArray arguments;
        arguments.reserve(arguments_.size());
        for (const auto& [argument_name, config] : arguments_) {
            JsonObject entry;
            entry.emplace("name", argument_name);
            entry.emplace("kind", to_string(config.kind));
            if (!config.description.empty()) {
                entry.emplace("description", config.description);
            }
            if (!config.value_label.empty()) {
                entry.emplace("label", config.value_label);
            }
            if (!config.custom_type_name.empty()) {
                entry.emplace("custom_type", config.custom_type_name);
            }
            if (config.optional) {
                entry.emplace("optional", true);
            }
            if (config.default_value.has_value()) {
                entry.emplace("default", format_value(*config.default_value));
            }
            if (!config.environment_variables.empty()) {
                entry.emplace("environment", JsonValue(detail::json_array_from_strings(config.environment_variables)));
            }
            if (!config.choices.empty()) {
                entry.emplace("choices", JsonValue(detail::json_array_from_strings(config.choices)));
            }
            if (!config.validators.empty()) {
                entry.emplace("validators", JsonValue(detail::json_array_from_validator_names(config.validators)));
            }
            if (!config.completion_values.empty()) {
                entry.emplace("completion_values",
                              JsonValue(detail::json_array_from_completion_values(config.completion_values)));
            }
            arguments.emplace_back(std::move(entry));
        }
        result.emplace("arguments", std::move(arguments));

        JsonArray options;
        const auto visible_option_items = visible_options();
        options.reserve(visible_option_items.size());
        for (const auto& option : visible_option_items) {
            const auto& config = *option.config;
            JsonObject entry;
            entry.emplace("name", option.name);
            entry.emplace("kind", to_string(config.kind));
            if (!config.description.empty()) {
                entry.emplace("description", config.description);
            }
            if (!config.value_label.empty()) {
                entry.emplace("label", config.value_label);
            }
            if (!config.custom_type_name.empty()) {
                entry.emplace("custom_type", config.custom_type_name);
            }
            if (!config.aliases.empty()) {
                entry.emplace("aliases", JsonValue(detail::json_array_from_strings(config.aliases)));
            }
            if (!config.group.empty()) {
                entry.emplace("group", config.group);
            }
            if (!config.exclusive_group.empty()) {
                entry.emplace("exclusive_group", config.exclusive_group);
            }
            if (config.optional) {
                entry.emplace("optional", true);
            }
            if (config.default_value.has_value()) {
                entry.emplace("default", format_value(*config.default_value));
            }
            if (!config.environment_variables.empty()) {
                entry.emplace("environment", JsonValue(detail::json_array_from_strings(config.environment_variables)));
            }
            if (!config.requires.empty()) {
                entry.emplace("requires", JsonValue(detail::json_array_from_strings(config.requires)));
            }
            if (!config.excludes.empty()) {
                entry.emplace("excludes", JsonValue(detail::json_array_from_strings(config.excludes)));
            }
            if (!config.choices.empty()) {
                entry.emplace("choices", JsonValue(detail::json_array_from_strings(config.choices)));
            }
            if (!config.validators.empty()) {
                entry.emplace("validators", JsonValue(detail::json_array_from_validator_names(config.validators)));
            }
            if (!config.completion_values.empty()) {
                entry.emplace("completion_values",
                              JsonValue(detail::json_array_from_completion_values(config.completion_values)));
            }
            if (!config.deprecated_message.empty()) {
                entry.emplace("deprecated", true);
                entry.emplace("deprecated_message", config.deprecated_message);
            }
            if (!config.deprecated_alias_messages.empty()) {
                JsonObject deprecated_aliases;
                for (const auto& [alias_name, message] : config.deprecated_alias_messages) {
                    deprecated_aliases.emplace(alias_name, message);
                }
                entry.emplace("deprecated_aliases", std::move(deprecated_aliases));
            }
            if (config.hidden) {
                entry.emplace("hidden", true);
            }
            options.emplace_back(std::move(entry));
        }
        result.emplace("options", std::move(options));

        JsonArray option_groups;
        for (const auto& [group_name, group] : option_groups_) {
            JsonObject entry;
            entry.emplace("name", group_name);
            if (!group.description.empty()) {
                entry.emplace("description", group.description);
            }
            option_groups.emplace_back(std::move(entry));
        }
        result.emplace("option_groups", std::move(option_groups));

        JsonArray commands;
        commands.reserve(commands_.size());
        for (const auto& [_, command_ptr] : commands_) {
            commands.emplace_back(command_ptr->schema());
        }
        result.emplace("commands", std::move(commands));

        return result;
    }

    [[nodiscard]] std::string schema_json() const { return JsonValue(schema()).dump(); }

    [[nodiscard]] std::string markdown() const {
        std::string out;
        append_markdown(out, command_path(), 1);
        while (!out.empty() && out.back() == '\n') {
            out.pop_back();
        }
        return out;
    }

    void ensure_option_group(const std::string& group_name) {
        option_groups_.try_emplace(group_name, OptionGroupDefinition{group_name, ""});
    }

    Command& argument(std::string argument_name, ArgumentConfig config = k_default_argument_config) {
        if (argument_name.empty()) {
            throw CommandError("Argument name cannot be empty");
        }

        if (find_argument(argument_name) != nullptr) {
            throw CommandError(
                "Argument is already defined",
                CommandErrorOptions{command_path_string(),
                                    "",
                                    argument_name,
                                    {},
                                    {},
                                    {},
                                    "Each argument name must be unique within the same command."});
        }

        if (!config.choices.empty()) {
            config.completion_values = completion_values_from_choices(config.choices);
        }

        if (config.default_value.has_value()) {
            config.optional = true;
            validate_default_value(argument_name,
                                   config.kind,
                                   *config.default_value,
                                   "argument",
                                   config.custom_type_name,
                                   static_cast<bool>(config.custom_parser));
        }

        if (!config.optional) {
            const auto has_optional_before =
                std::any_of(arguments_.begin(), arguments_.end(), [](const auto& entry) {
                    return entry.second.optional || entry.second.default_value.has_value();
                });

            if (has_optional_before) {
                throw CommandError(
                    "Required arguments must be declared before optional ones",
                    CommandErrorOptions{command_path_string(),
                                        "",
                                        argument_name,
                                        {},
                                        {},
                                        {},
                                        "Define all required positional arguments first."});
            }
        }

        arguments_.emplace_back(std::move(argument_name), std::move(config));
        return *this;
    }

    ArgumentBuilder arg(std::string argument_name, ValueKind kind = ValueKind::string);

    Command& option(std::string option_name, OptionConfig config = k_default_option_config) {
        if (option_name.empty()) {
            throw CommandError("Option name cannot be empty");
        }

        if (find_option(option_name).has_value()) {
            throw CommandError(
                "Option is already defined",
                CommandErrorOptions{command_path_string(),
                                    detail::prefixed_option_name(option_name),
                                    "",
                                    {},
                                    {},
                                    {},
                                    "Each option name must be unique within the same command."});
        }

        for (const auto& alias_name : config.aliases) {
            if (alias_name.empty()) {
                throw CommandError("Option aliases cannot be empty");
            }

            if (alias_name == option_name || alias_matches_existing_option(alias_name)) {
                throw CommandError(
                    "Option alias collides with an existing option",
                    CommandErrorOptions{command_path_string(),
                                        detail::prefixed_alias_name(alias_name),
                                        "",
                                        {},
                                        {},
                                        {},
                                        "Use unique short aliases for each option."});
            }
        }

        if (!config.choices.empty()) {
            config.completion_values = completion_values_from_choices(config.choices);
        }

        if (config.group.empty()) {
            config.group = "Options";
        }
        option_groups_.try_emplace(config.group, OptionGroupDefinition{config.group, ""});

        if (config.default_value.has_value()) {
            config.optional = true;
            validate_default_value(option_name,
                                   config.kind,
                                   *config.default_value,
                                   "option",
                                   config.custom_type_name,
                                   static_cast<bool>(config.custom_parser));
        }

        options_.emplace_back(std::move(option_name), std::move(config));
        return *this;
    }

    OptionBuilder opt(std::string option_name, ValueKind kind = ValueKind::boolean);

    OptionGroup option_group(std::string group_name, std::string description = {}) {
        if (group_name.empty()) {
            throw CommandError("Option group name cannot be empty");
        }

        option_groups_[group_name] = OptionGroupDefinition{group_name, std::move(description)};
        return OptionGroup(this, group_name);
    }

    Command& action(CommandHandler callback) {
        if (handler_) {
            throw CommandError(
                "This command already has an action handler",
                CommandErrorOptions{command_path_string(),
                                    "",
                                    "",
                                    {},
                                    {},
                                    {},
                                    "Each command can only register one action handler."});
        }

        handler_ = std::move(callback);
        return *this;
    }

    Command& command(std::string command_name) {
        if (command_name.empty()) {
            throw CommandError("Command name cannot be empty");
        }

        if (detail::contains_key(commands_, command_name)) {
            throw CommandError(
                "Subcommand is already defined",
                CommandErrorOptions{command_path_string() + " " + command_name,
                                    "",
                                    "",
                                    {},
                                    {},
                                    {},
                                    "Each subcommand must have a unique name within the same command."});
        }

        auto child = std::make_unique<Command>(command_name);
        child->parent_ = this;
        auto* raw = child.get();
        commands_.emplace(command_name, std::move(child));
        return *raw;
    }

    struct SubcommandLookup {
        const Command* command = nullptr;
        bool used_alias = false;
        std::string matched_token;
        std::optional<std::string> deprecated_alias_message;
    };

    [[nodiscard]] Command* find_subcommand_mutable(std::string_view token) {
        const auto direct = commands_.find(std::string(token));
        return direct == commands_.end() ? nullptr : direct->second.get();
    }

    [[nodiscard]] Command& ensure_command(std::string command_name) {
        if (auto* existing = find_subcommand_mutable(command_name); existing != nullptr) {
            return *existing;
        }

        return command(std::move(command_name));
    }

    [[nodiscard]] std::optional<SubcommandLookup> find_subcommand_lookup(std::string_view token) const {
        const auto direct = commands_.find(std::string(token));
        if (direct != commands_.end()) {
            return SubcommandLookup{direct->second.get(), false, std::string(token), std::nullopt};
        }

        for (const auto& [_, command_ptr] : commands_) {
            if (std::find(command_ptr->aliases_.begin(), command_ptr->aliases_.end(), token) !=
                command_ptr->aliases_.end()) {
                std::optional<std::string> deprecated_message;
                const auto deprecated_alias = command_ptr->deprecated_alias_messages_.find(std::string(token));
                if (deprecated_alias != command_ptr->deprecated_alias_messages_.end()) {
                    deprecated_message = deprecated_alias->second;
                }
                return SubcommandLookup{command_ptr.get(), true, std::string(token), std::move(deprecated_message)};
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] const Command* find_subcommand(std::string_view token) const {
        if (const auto lookup = find_subcommand_lookup(token); lookup.has_value()) {
            return lookup->command;
        }
        return nullptr;
    }

    [[nodiscard]] std::optional<std::pair<std::string, const OptionConfig*>> find_option(
        std::string_view token) const {
        for (const auto& [name, config] : options_) {
            if (name == token) {
                return std::pair<std::string, const OptionConfig*>{name, &config};
            }

            if (std::find(config.aliases.begin(), config.aliases.end(), token) != config.aliases.end()) {
                return std::pair<std::string, const OptionConfig*>{name, &config};
            }
        }

        return std::nullopt;
    }

    struct OptionLookup {
        const Command* owner = nullptr;
        std::string name;
        const OptionConfig* config = nullptr;
        bool used_alias = false;
        std::string matched_token;
        std::optional<std::string> deprecated_alias_message;
    };

    [[nodiscard]] std::optional<OptionLookup> find_visible_option(std::string_view token) const {
        for (auto current = this; current != nullptr; current = current->parent_) {
            if (const auto option = current->find_option(token); option.has_value()) {
                std::optional<std::string> deprecated_message;
                const auto used_alias = option->first != token;
                if (used_alias) {
                    const auto deprecated_alias = option->second->deprecated_alias_messages.find(std::string(token));
                    if (deprecated_alias != option->second->deprecated_alias_messages.end()) {
                        deprecated_message = deprecated_alias->second;
                    }
                }
                return OptionLookup{
                    current, option->first, option->second, used_alias, std::string(token), std::move(deprecated_message)};
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<OptionLookup> visible_options() const {
        std::vector<OptionLookup> visible;
        std::vector<const Command*> lineage;

        for (auto current = this; current != nullptr; current = current->parent_) {
            lineage.push_back(current);
        }
        std::reverse(lineage.begin(), lineage.end());

        for (const auto* command : lineage) {
            for (const auto& [name, config] : command->options_) {
                const auto duplicate = std::find_if(visible.begin(),
                                                    visible.end(),
                                                    [&name](const OptionLookup& lookup) {
                                                        return lookup.name == name;
                                                    });
                if (duplicate == visible.end()) {
                    visible.push_back(OptionLookup{command, name, &config, false, name, std::nullopt});
                } else {
                    *duplicate = OptionLookup{command, name, &config, false, name, std::nullopt};
                }
            }
        }

        return visible;
    }

    [[nodiscard]] const ArgumentConfig* find_argument(std::string_view name) const {
        for (const auto& [argument_name, config] : arguments_) {
            if (argument_name == name) {
                return &config;
            }
        }

        return nullptr;
    }

    [[nodiscard]] ArgumentConfig* find_argument_mutable(std::string_view name) {
        for (auto& entry : arguments_) {
            if (entry.first == name) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const OptionConfig* find_option_config(std::string_view name) const {
        if (const auto option = find_option(name); option.has_value()) {
            return option->second;
        }
        return nullptr;
    }

    [[nodiscard]] const OptionConfig* find_visible_option_config(std::string_view name) const {
        if (const auto option = find_visible_option(name); option.has_value()) {
            return option->config;
        }
        return nullptr;
    }

    [[nodiscard]] OptionConfig* find_option_config_mutable(std::string_view name) {
        for (auto& entry : options_) {
            if (entry.first == name) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    [[nodiscard]] std::vector<CompletionSuggestion> complete_argument(std::size_t index,
                                                                      std::string_view prefix) const {
        if (index >= arguments_.size()) {
            return {};
        }

        const auto& [_, config] = arguments_[index];
        return detail::resolve_value_completion(
            config.kind, config.choices, config.completion_values, config.completion_provider, prefix);
    }

    [[nodiscard]] std::vector<CompletionSuggestion> complete_option_value(std::string_view option_name,
                                                                          std::string_view prefix) const {
        const auto* config = find_visible_option_config(option_name);
        if (config == nullptr) {
            return {};
        }

        return detail::resolve_value_completion(
            config->kind, config->choices, config->completion_values, config->completion_provider, prefix);
    }

    [[nodiscard]] std::vector<CompletionSuggestion> complete_subcommands(std::string_view prefix) const {
        std::vector<CompletionSuggestion> suggestions;
        for (const auto& [name, command_ptr] : commands_) {
            const auto description = command_ptr->deprecated_message_.empty()
                                         ? command_ptr->description_
                                         : command_ptr->description_.empty()
                                               ? "deprecated"
                                               : command_ptr->description_ + " (deprecated)";
            suggestions.push_back({name, description});
            for (const auto& alias_name : command_ptr->aliases_) {
                auto alias_description = std::string("alias for ") + name;
                const auto deprecated_alias = command_ptr->deprecated_alias_messages_.find(alias_name);
                if (deprecated_alias != command_ptr->deprecated_alias_messages_.end()) {
                    alias_description += " (deprecated)";
                }
                suggestions.push_back({alias_name, std::move(alias_description)});
            }
        }

        filter_completion_suggestions(suggestions, prefix);
        return suggestions;
    }

    [[nodiscard]] std::vector<CompletionSuggestion> complete_options(std::string_view prefix) const {
        std::vector<CompletionSuggestion> suggestions;

        for (const auto& option : visible_options()) {
            if (option.config->hidden) {
                continue;
            }

            suggestions.push_back({detail::prefixed_option_name(option.name), option.config->description});
            if (option.config->kind == ValueKind::boolean) {
                suggestions.push_back({"--no-" + option.name, "Disable " + option.name});
            }

            for (const auto& alias_name : option.config->aliases) {
                auto description = option.config->description;
                if (detail::contains_key(option.config->deprecated_alias_messages, alias_name)) {
                    description = description.empty() ? "deprecated alias" : description + " (deprecated alias)";
                }
                suggestions.push_back({detail::prefixed_alias_name(alias_name), std::move(description)});
            }
        }

        filter_completion_suggestions(suggestions, prefix);
        return suggestions;
    }

    [[nodiscard]] CliValue parse_argument_value(std::string_view argument_name,
                                                std::string_view raw) const {
        const auto* config = find_argument(argument_name);
        if (config == nullptr) {
            throw CommandError(
                "Unknown argument",
                CommandErrorOptions{command_path_string(), "", std::string(argument_name)});
        }

        CliValue value = config->custom_parser ? config->custom_parser(raw)
                                               : coerce_argument(config->kind, raw, config->choices);
        validate_argument_value(argument_name, value);
        return value;
    }

    [[nodiscard]] CliValue parse_option_value(std::string_view option_name,
                                              const std::string& raw) const {
        const auto* config = find_visible_option_config(option_name);
        if (config == nullptr) {
            throw CommandError(
                "Unknown option",
                CommandErrorOptions{command_path_string(), std::string(option_name), ""});
        }

        CliValue value = config->custom_parser ? config->custom_parser(raw)
                                               : coerce_option(config->kind, raw, config->choices);
        validate_option_value(option_name, value);
        return value;
    }

    void validate_argument_value(std::string_view argument_name, const CliValue& value) const {
        const auto* config = find_argument(argument_name);
        if (config == nullptr) {
            return;
        }

        apply_validators(config->validators,
                         value,
                         ValidatorContext{command_path_string(), std::string(argument_name), false, config->kind});
    }

    void validate_option_value(std::string_view option_name, const CliValue& value) const {
        const auto* config = find_visible_option_config(option_name);
        if (config == nullptr) {
            return;
        }

        apply_validators(config->validators,
                         value,
                         ValidatorContext{command_path_string(), std::string(option_name), true, config->kind});
    }

    [[nodiscard]] std::string help(std::vector<std::string> path = {}) const {
        if (path.empty()) {
            path = command_path();
        }

        std::vector<std::string> lines;
        if (!description_.empty()) {
            lines.push_back(description_);
            lines.emplace_back();
        }

        if (!deprecated_message_.empty()) {
            lines.push_back("Deprecated: " + deprecated_message_);
            lines.emplace_back();
        }

        lines.push_back("Usage:");
        const auto usage_text = usage();
        const auto command_text = detail::join_strings(path, " ");
        lines.push_back(usage_text.empty() ? "  " + command_text : "  " + command_text + " " + usage_text);
        lines.emplace_back();

        const auto print_section =
            [&lines](std::string_view header, const std::vector<std::pair<std::string, std::string>>& items) {
                if (items.empty()) {
                    return;
                }

                lines.push_back(std::string(header) + ":");

                std::size_t width = 0;
                for (const auto& item : items) {
                    width = std::max(width, item.first.size());
                }

                for (const auto& item : items) {
                    if (item.second.empty()) {
                        lines.push_back("  " + item.first);
                    } else {
                        lines.push_back("  " + detail::pad_right(item.first, width) + "  " + item.second);
                    }
                }

                lines.emplace_back();
            };

        std::vector<std::pair<std::string, std::string>> argument_items;
        for (const auto& [name, config] : arguments_) {
            const auto label = detail::default_value_kind_label(config);
            const auto left =
                config.optional ? "[" + name + "] <" + label + ">" : "<" + name + "> <" + label + ">";

            std::vector<std::string> description_parts;
            if (!config.description.empty()) {
                description_parts.push_back(config.description);
            }
            if (config.optional) {
                description_parts.push_back("optional");
            }
            if (config.default_value.has_value()) {
                description_parts.push_back("default: " + format_value(*config.default_value));
            }
            if (!config.environment_variables.empty()) {
                description_parts.push_back("env: " + detail::join_strings(config.environment_variables, ", "));
            }
            if (!config.custom_type_name.empty()) {
                description_parts.push_back("type: " + config.custom_type_name);
            }
            if (!config.choices.empty()) {
                description_parts.push_back("choices: " + detail::join_strings(config.choices, ", "));
            }
            if (!config.validators.empty()) {
                std::vector<std::string> names;
                names.reserve(config.validators.size());
                for (const auto& validator : config.validators) {
                    if (!validator.name.empty()) {
                        names.push_back(validator.name);
                    }
                }
                if (!names.empty()) {
                    description_parts.push_back("validators: " + detail::join_strings(names, ", "));
                }
            }

            argument_items.emplace_back(left, detail::join_strings(description_parts, " | "));
        }
        print_section("Arguments", argument_items);

        std::vector<const Command*> lineage;
        for (auto current = this; current != nullptr; current = current->parent_) {
            lineage.push_back(current);
        }
        std::reverse(lineage.begin(), lineage.end());

        std::map<std::string, std::string> visible_groups;
        for (const auto* command : lineage) {
            for (const auto& entry : command->option_groups_) {
                visible_groups[entry.first] = entry.second.description;
            }
        }

        const auto visible_option_items = visible_options();
        for (const auto& group : visible_groups) {
            const auto& group_name = group.first;
            std::vector<std::pair<std::string, std::string>> option_items;
            for (const auto& option : visible_option_items) {
                const auto& name = option.name;
                const auto& config = *option.config;

                if (config.hidden || config.group != group_name) {
                    continue;
                }

                std::string aliases;
                if (!config.aliases.empty()) {
                    for (std::size_t index = 0; index < config.aliases.size(); ++index) {
                        if (index > 0) {
                            aliases += ", ";
                        }
                        aliases += detail::prefixed_alias_name(config.aliases[index]);
                    }
                    aliases += ", ";
                }

                const auto value_hint =
                    config.kind == ValueKind::boolean ? std::string()
                                                      : " <" + detail::default_value_kind_label(config) + ">";
                const auto left = aliases + detail::prefixed_option_name(name) + value_hint;

                std::vector<std::string> description_parts;
                if (!config.description.empty()) {
                    description_parts.push_back(config.description);
                }
                description_parts.push_back(config.optional ? "optional" : "required");
                if (config.default_value.has_value()) {
                    description_parts.push_back("default: " + format_value(*config.default_value));
                }
                if (!config.environment_variables.empty()) {
                    description_parts.push_back("env: " + detail::join_strings(config.environment_variables, ", "));
                }
                if (!config.custom_type_name.empty()) {
                    description_parts.push_back("type: " + config.custom_type_name);
                }
                if (!config.choices.empty()) {
                    description_parts.push_back("choices: " + detail::join_strings(config.choices, ", "));
                }
                if (!config.requires.empty()) {
                    std::vector<std::string> required_options;
                    for (const auto& value : config.requires) {
                        required_options.push_back(detail::prefixed_option_name(value));
                    }
                    description_parts.push_back("requires: " + detail::join_strings(required_options, ", "));
                }
                if (!config.excludes.empty()) {
                    std::vector<std::string> excluded_options;
                    for (const auto& value : config.excludes) {
                        excluded_options.push_back(detail::prefixed_option_name(value));
                    }
                    description_parts.push_back("excludes: " + detail::join_strings(excluded_options, ", "));
                }
                if (!config.exclusive_group.empty()) {
                    description_parts.push_back("exclusive group: " + config.exclusive_group);
                }
                if (!config.deprecated_message.empty()) {
                    description_parts.push_back("deprecated: " + config.deprecated_message);
                }
                if (!config.deprecated_alias_messages.empty()) {
                    std::vector<std::string> deprecated_aliases;
                    deprecated_aliases.reserve(config.deprecated_alias_messages.size());
                    for (const auto& [alias_name, message] : config.deprecated_alias_messages) {
                        deprecated_aliases.push_back(detail::prefixed_alias_name(alias_name) + " (" + message + ")");
                    }
                    description_parts.push_back("deprecated aliases: " +
                                                detail::join_strings(deprecated_aliases, ", "));
                }
                if (!config.validators.empty()) {
                    std::vector<std::string> names;
                    names.reserve(config.validators.size());
                    for (const auto& validator : config.validators) {
                        if (!validator.name.empty()) {
                            names.push_back(validator.name);
                        }
                    }
                    if (!names.empty()) {
                        description_parts.push_back("validators: " + detail::join_strings(names, ", "));
                    }
                }

                option_items.emplace_back(left, detail::join_strings(description_parts, " | "));
            }

            if (!group.second.empty() && !option_items.empty()) {
                lines.push_back(group.second);
            }
            print_section(group_name, option_items);
        }

        std::vector<std::pair<std::string, std::string>> command_items;
        for (const auto& [name, command_ptr] : commands_) {
            const auto alias_text = command_ptr->aliases_.empty()
                                        ? std::string()
                                        : " (" + detail::join_strings(command_ptr->aliases_, ", ") + ")";

            std::vector<std::string> description_parts;
            if (!command_ptr->description_.empty()) {
                description_parts.push_back(command_ptr->description_);
            }
            if (!command_ptr->deprecated_message_.empty()) {
                description_parts.push_back("deprecated: " + command_ptr->deprecated_message_);
            }
            if (!command_ptr->deprecated_alias_messages_.empty()) {
                std::vector<std::string> deprecated_aliases;
                deprecated_aliases.reserve(command_ptr->deprecated_alias_messages_.size());
                for (const auto& [alias_name, message] : command_ptr->deprecated_alias_messages_) {
                    deprecated_aliases.push_back(alias_name + " (" + message + ")");
                }
                description_parts.push_back("deprecated aliases: " +
                                            detail::join_strings(deprecated_aliases, ", "));
            }

            command_items.emplace_back(name + alias_text, detail::join_strings(description_parts, " | "));
        }
        print_section("Commands", command_items);

        std::string result;
        for (std::size_t index = 0; index < lines.size(); ++index) {
            result += lines[index];
            if (index + 1 < lines.size()) {
                result.push_back('\n');
            }
        }

        while (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }

        return result;
    }

    [[nodiscard]] std::string usage() const {
        std::vector<std::string> tokens;
        if (!commands_.empty()) {
            tokens.emplace_back("<command>");
        }
        for (const auto& [name, config] : arguments_) {
            tokens.push_back(config.optional ? "[" + name + "]" : "<" + name + ">");
        }

        if (!visible_options().empty()) {
            tokens.emplace_back("[options]");
        }

        return detail::join_strings(tokens, " ");
    }

protected:
    [[nodiscard]] static std::vector<CompletionSuggestion> completion_values_from_choices(
        const std::vector<std::string>& choices) {
        std::vector<CompletionSuggestion> suggestions;
        suggestions.reserve(choices.size());
        for (const auto& choice : choices) {
            suggestions.push_back({choice, "choice"});
        }
        return suggestions;
    }

private:
    void validate_command_alias_available(std::string_view alias_name) const {
        if (alias_name == name_) {
            throw CommandError(
                "Command alias collides with the command name",
                CommandErrorOptions{command_path_string(),
                                    "",
                                    std::string(alias_name),
                                    {},
                                    {std::string(alias_name)},
                                    {},
                                    "Use a distinct alias for the command."});
        }

        if (parent_ == nullptr) {
            return;
        }

        for (const auto& [sibling_name, sibling_ptr] : parent_->commands_) {
            if (sibling_ptr.get() == this) {
                continue;
            }

            if (sibling_name == alias_name ||
                std::find(sibling_ptr->aliases_.begin(), sibling_ptr->aliases_.end(), alias_name) !=
                    sibling_ptr->aliases_.end()) {
                throw CommandError(
                    "Command alias collides with an existing subcommand",
                    CommandErrorOptions{parent_->command_path_string(),
                                        "",
                                        std::string(alias_name),
                                        {},
                                        {std::string(alias_name)},
                                        {},
                                        "Use unique aliases across sibling commands."});
            }
        }
    }

    [[nodiscard]] bool alias_matches_existing_option(std::string_view alias_name) const {
        for (const auto& [name, config] : options_) {
            if (name == alias_name) {
                return true;
            }
            if (std::find(config.aliases.begin(), config.aliases.end(), alias_name) != config.aliases.end()) {
                return true;
            }
        }
        return false;
    }

    void ensure_option_alias_available(std::string_view option_name, std::string_view alias_name) const {
        if (alias_name.empty()) {
            throw CommandError("Option aliases cannot be empty");
        }
        if (alias_name == option_name) {
            throw CommandError(
                "Option alias collides with the option name",
                CommandErrorOptions{command_path_string(),
                                    detail::prefixed_alias_name(alias_name),
                                    "",
                                    {},
                                    {std::string(alias_name)},
                                    {},
                                    "Use a distinct alias for the option."});
        }

        for (const auto& [existing_name, config] : options_) {
            if (existing_name != option_name && existing_name == alias_name) {
                throw CommandError(
                    "Option alias collides with an existing option",
                    CommandErrorOptions{command_path_string(),
                                        detail::prefixed_alias_name(alias_name),
                                        "",
                                        {},
                                        {std::string(alias_name)},
                                        {},
                                        "Use unique short aliases for each option."});
            }

            for (const auto& existing_alias : config.aliases) {
                if (existing_alias == alias_name) {
                    throw CommandError(
                        "Option alias collides with an existing option",
                        CommandErrorOptions{command_path_string(),
                                            detail::prefixed_alias_name(alias_name),
                                            "",
                                            {},
                                            {std::string(alias_name)},
                                            {},
                                            "Use unique short aliases for each option."});
                }
            }
        }
    }

    static void validate_default_value(const std::string& name,
                                       ValueKind kind,
                                       const CliValue& value,
                                       std::string_view label,
                                       std::string_view custom_type_name = {},
                                       bool custom_parser = false) {
        if (custom_parser) {
            const auto* typed = std::get_if<TypedValuePtr>(&value);
            if (typed != nullptr && *typed != nullptr &&
                (custom_type_name.empty() || (*typed)->type_name == custom_type_name)) {
                return;
            }

            throw CommandError(
                "Default value does not match the configured custom parser",
                CommandErrorOptions{"",
                                    "",
                                    "",
                                    {std::string(custom_type_name.empty() ? "custom" : custom_type_name)},
                                    {format_value(value)},
                                    {},
                                    "The default " + std::string(label) + " \"" + name +
                                        "\" must match the declared custom parser type."});
        }

        if (!matches_value_kind(value, kind)) {
            throw CommandError(
                "Default value does not match the configured kind",
                CommandErrorOptions{"",
                                    "",
                                    "",
                                    {to_string(kind)},
                                    {format_value(value)},
                                    {},
                                    "The default " + std::string(label) + " \"" + name +
                                        "\" must match the declared value kind."});
        }
    }

    void append_markdown(std::string& out, std::vector<std::string> path, std::size_t depth) const {
        const auto heading_level = std::min<std::size_t>(depth, 6);
        out.append(heading_level, '#');
        out += " ";
        out += detail::join_strings(path, " ");
        out += "\n\n";

        if (!description_.empty()) {
            out += description_;
            out += "\n\n";
        }

        if (!aliases_.empty()) {
            detail::append_markdown_bullet(out, "Aliases", detail::join_strings(aliases_, ", "));
        }
        if (!deprecated_message_.empty()) {
            detail::append_markdown_bullet(out, "Deprecated", deprecated_message_);
        }
        if (!deprecated_alias_messages_.empty()) {
            std::vector<std::string> deprecated_aliases;
            deprecated_aliases.reserve(deprecated_alias_messages_.size());
            for (const auto& [alias_name, message] : deprecated_alias_messages_) {
                deprecated_aliases.push_back(alias_name + " (" + message + ")");
            }
            detail::append_markdown_bullet(out, "Deprecated aliases", detail::join_strings(deprecated_aliases, ", "));
        }
        detail::append_markdown_bullet(out, "Usage", detail::join_strings(path, " ") +
                                                        (usage().empty() ? std::string() : " " + usage()));
        if (allows_passthrough()) {
            detail::append_markdown_bullet(out, "Passthrough", "This command accepts unknown options and extra arguments.");
        }
        out += '\n';

        if (!arguments_.empty()) {
            out += "Arguments\n\n";
            out += "| Name | Type | Details |\n";
            out += "| --- | --- | --- |\n";
            for (const auto& [argument_name, config] : arguments_) {
                std::vector<std::string> details;
                if (!config.description.empty()) {
                    details.push_back(config.description);
                }
                if (config.optional) {
                    details.push_back("optional");
                }
                if (config.default_value.has_value()) {
                    details.push_back("default: `" + format_value(*config.default_value) + "`");
                }
                if (!config.environment_variables.empty()) {
                    details.push_back("env: `" + detail::join_strings(config.environment_variables, "`, `") + "`");
                }
                if (!config.choices.empty()) {
                    details.push_back("choices: `" + detail::join_strings(config.choices, "`, `") + "`");
                }
                if (!config.validators.empty()) {
                    std::vector<std::string> names;
                    for (const auto& validator : config.validators) {
                        if (!validator.name.empty()) {
                            names.push_back(validator.name);
                        }
                    }
                    if (!names.empty()) {
                        details.push_back("validators: `" + detail::join_strings(names, "`, `") + "`");
                    }
                }

                out += "| `" + argument_name + "` | `" + detail::default_value_kind_label(config) + "` | " +
                       detail::markdown_escape(detail::join_strings(details, " | ")) + " |\n";
            }
            out += "\n";
        }

        const auto visible_option_items = visible_options();
        if (!visible_option_items.empty()) {
            out += "Options\n\n";
            out += "| Name | Type | Details |\n";
            out += "| --- | --- | --- |\n";
            for (const auto& option : visible_option_items) {
                const auto& config = *option.config;
                if (config.hidden) {
                    continue;
                }

                std::vector<std::string> names = {"--" + option.name};
                for (const auto& alias_name : config.aliases) {
                    names.push_back("-" + alias_name);
                }

                std::vector<std::string> details;
                if (!config.description.empty()) {
                    details.push_back(config.description);
                }
                details.push_back(config.optional ? "optional" : "required");
                if (!config.group.empty() && config.group != "Options") {
                    details.push_back("group: " + config.group);
                }
                if (config.default_value.has_value()) {
                    details.push_back("default: `" + format_value(*config.default_value) + "`");
                }
                if (!config.environment_variables.empty()) {
                    details.push_back("env: `" + detail::join_strings(config.environment_variables, "`, `") + "`");
                }
                if (!config.choices.empty()) {
                    details.push_back("choices: `" + detail::join_strings(config.choices, "`, `") + "`");
                }
                if (!config.requires.empty()) {
                    details.push_back("requires: `" + detail::join_strings(config.requires, "`, `") + "`");
                }
                if (!config.excludes.empty()) {
                    details.push_back("excludes: `" + detail::join_strings(config.excludes, "`, `") + "`");
                }
                if (!config.exclusive_group.empty()) {
                    details.push_back("exclusive group: `" + config.exclusive_group + "`");
                }
                if (!config.deprecated_message.empty()) {
                    details.push_back("deprecated: " + config.deprecated_message);
                }
                if (!config.deprecated_alias_messages.empty()) {
                    std::vector<std::string> deprecated_aliases;
                    for (const auto& [alias_name, message] : config.deprecated_alias_messages) {
                        deprecated_aliases.push_back("-" + alias_name + " (" + message + ")");
                    }
                    details.push_back("deprecated aliases: " + detail::join_strings(deprecated_aliases, ", "));
                }
                if (!config.validators.empty()) {
                    std::vector<std::string> names;
                    for (const auto& validator : config.validators) {
                        if (!validator.name.empty()) {
                            names.push_back(validator.name);
                        }
                    }
                    if (!names.empty()) {
                        details.push_back("validators: `" + detail::join_strings(names, "`, `") + "`");
                    }
                }

                out += "| `" + detail::join_strings(names, "`, `") + "` | `" +
                       detail::default_value_kind_label(config) + "` | " +
                       detail::markdown_escape(detail::join_strings(details, " | ")) + " |\n";
            }
            out += "\n";
        }

        if (!commands_.empty()) {
            out += "Commands\n\n";
            out += "| Command | Details |\n";
            out += "| --- | --- |\n";
            for (const auto& [command_name, command_ptr] : commands_) {
                std::vector<std::string> details;
                if (!command_ptr->description_.empty()) {
                    details.push_back(command_ptr->description_);
                }
                if (!command_ptr->aliases_.empty()) {
                    details.push_back("aliases: `" + detail::join_strings(command_ptr->aliases_, "`, `") + "`");
                }
                if (!command_ptr->deprecated_message_.empty()) {
                    details.push_back("deprecated: " + command_ptr->deprecated_message_);
                }
                if (!command_ptr->deprecated_alias_messages_.empty()) {
                    std::vector<std::string> deprecated_aliases;
                    for (const auto& [alias_name, message] : command_ptr->deprecated_alias_messages_) {
                        deprecated_aliases.push_back(alias_name + " (" + message + ")");
                    }
                    details.push_back("deprecated aliases: " + detail::join_strings(deprecated_aliases, ", "));
                }
                out += "| `" + command_name + "` | " + detail::markdown_escape(detail::join_strings(details, " | ")) +
                       " |\n";
            }
            out += "\n";
        }

        for (const auto& [command_name, command_ptr] : commands_) {
            auto child_path = path;
            child_path.push_back(command_name);
            command_ptr->append_markdown(out, std::move(child_path), depth + 1);
        }
    }

    std::string name_;
    std::string description_;
    std::vector<std::string> aliases_;
    std::string deprecated_message_;
    std::unordered_map<std::string, std::string> deprecated_alias_messages_;
    std::vector<std::pair<std::string, ArgumentConfig>> arguments_;
    std::vector<std::pair<std::string, OptionConfig>> options_;
    std::map<std::string, std::unique_ptr<Command>> commands_;
    std::map<std::string, OptionGroupDefinition> option_groups_;
    CommandHandler handler_;
    bool allow_unknown_options_ = false;
    bool allow_extra_arguments_ = false;
    Command* parent_ = nullptr;

    friend class OptionGroup;
    friend class ArgumentBuilder;
    friend class OptionBuilder;
};

class ArgumentBuilder {
public:
    ArgumentBuilder(Command* owner, std::string name)
        : owner_(owner)
        , name_(std::move(name)) {}

    ArgumentBuilder& description(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->description = std::move(value);
        }
        return *this;
    }

    ArgumentBuilder& label(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->value_label = std::move(value);
        }
        return *this;
    }

    ArgumentBuilder& optional(bool value = true) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->optional = value;
        }
        return *this;
    }

    ArgumentBuilder& required() { return optional(false); }

    ArgumentBuilder& default_value(CliValue value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->default_value = std::move(value);
            config->optional = true;
            if (owner_ != nullptr && config->default_value.has_value()) {
                Command::validate_default_value(name_,
                                               config->kind,
                                               *config->default_value,
                                               "argument",
                                               config->custom_type_name,
                                               static_cast<bool>(config->custom_parser));
            }
        }
        return *this;
    }

    ArgumentBuilder& env(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->environment_variables.push_back(std::move(value));
        }
        return *this;
    }

    ArgumentBuilder& choices(std::vector<std::string> values) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->choices = std::move(values);
            config->completion_values.clear();
            for (const auto& choice : config->choices) {
                config->completion_values.push_back({choice, "choice"});
            }
        }
        return *this;
    }

    ArgumentBuilder& complete(std::vector<CompletionSuggestion> values) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->completion_values = std::move(values);
        }
        return *this;
    }

    ArgumentBuilder& complete(CompletionProvider provider) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->completion_provider = std::move(provider);
        }
        return *this;
    }

    ArgumentBuilder& validate(ValidatorSpec validator) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->validators.push_back(std::move(validator));
        }
        return *this;
    }

    template <typename T, typename Parser>
    ArgumentBuilder& parse_as(Parser parser, std::string type_name = {}) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->custom_type_name = type_name.empty() ? std::string("custom") : std::move(type_name);
            config->custom_parser = detail::make_typed_parser<T>(std::move(parser), config->custom_type_name);
            if (config->default_value.has_value()) {
                Command::validate_default_value(name_,
                                               config->kind,
                                               *config->default_value,
                                               "argument",
                                               config->custom_type_name,
                                               true);
            }
        }
        return *this;
    }

    template <typename T, typename Parser, typename Formatter>
    ArgumentBuilder& parse_as(Parser parser, std::string type_name, Formatter formatter) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->custom_type_name = type_name.empty() ? std::string("custom") : std::move(type_name);
            config->custom_parser =
                detail::make_typed_parser<T>(std::move(parser), std::move(formatter), config->custom_type_name);
            if (config->default_value.has_value()) {
                Command::validate_default_value(name_,
                                               config->kind,
                                               *config->default_value,
                                               "argument",
                                               config->custom_type_name,
                                               true);
            }
        }
        return *this;
    }

private:
    [[nodiscard]] ArgumentConfig* config_mutable() const {
        return owner_ == nullptr ? nullptr : owner_->find_argument_mutable(name_);
    }

    Command* owner_ = nullptr;
    std::string name_;
};

class OptionBuilder {
public:
    OptionBuilder(Command* owner, std::string name)
        : owner_(owner)
        , name_(std::move(name)) {}

    OptionBuilder& alias(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            if (owner_ != nullptr) {
                owner_->ensure_option_alias_available(name_, value);
            }
            config->aliases.push_back(std::move(value));
        }
        return *this;
    }

    OptionBuilder& deprecated_alias(std::string value,
                                    std::string message = "This option alias is deprecated.") {
        if (auto* config = config_mutable(); config != nullptr) {
            if (owner_ != nullptr) {
                owner_->ensure_option_alias_available(name_, value);
            }
            config->aliases.push_back(value);
            config->deprecated_alias_messages[std::move(value)] = std::move(message);
        }
        return *this;
    }

    OptionBuilder& description(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->description = std::move(value);
        }
        return *this;
    }

    OptionBuilder& label(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->value_label = std::move(value);
        }
        return *this;
    }

    OptionBuilder& group(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->group = std::move(value);
            if (owner_ != nullptr) {
                owner_->ensure_option_group(config->group);
            }
        }
        return *this;
    }

    OptionBuilder& optional(bool value = true) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->optional = value;
        }
        return *this;
    }

    OptionBuilder& required() { return optional(false); }

    OptionBuilder& default_value(CliValue value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->default_value = std::move(value);
            config->optional = true;
            if (config->default_value.has_value()) {
                Command::validate_default_value(name_,
                                               config->kind,
                                               *config->default_value,
                                               "option",
                                               config->custom_type_name,
                                               static_cast<bool>(config->custom_parser));
            }
        }
        return *this;
    }

    OptionBuilder& env(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->environment_variables.push_back(std::move(value));
        }
        return *this;
    }

    OptionBuilder& choices(std::vector<std::string> values) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->choices = std::move(values);
            config->completion_values.clear();
            for (const auto& choice : config->choices) {
                config->completion_values.push_back({choice, "choice"});
            }
        }
        return *this;
    }

    OptionBuilder& complete(std::vector<CompletionSuggestion> values) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->completion_values = std::move(values);
        }
        return *this;
    }

    OptionBuilder& complete(CompletionProvider provider) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->completion_provider = std::move(provider);
        }
        return *this;
    }

    OptionBuilder& validate(ValidatorSpec validator) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->validators.push_back(std::move(validator));
        }
        return *this;
    }

    OptionBuilder& requires(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->requires.push_back(std::move(value));
        }
        return *this;
    }

    OptionBuilder& excludes(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->excludes.push_back(std::move(value));
        }
        return *this;
    }

    OptionBuilder& exclusive_group(std::string value) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->exclusive_group = std::move(value);
        }
        return *this;
    }

    OptionBuilder& hidden(bool value = true) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->hidden = value;
        }
        return *this;
    }

    OptionBuilder& deprecated(std::string message = "This option is deprecated.") {
        if (auto* config = config_mutable(); config != nullptr) {
            config->deprecated_message = std::move(message);
        }
        return *this;
    }

    template <typename T, typename Parser>
    OptionBuilder& parse_as(Parser parser, std::string type_name = {}) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->custom_type_name = type_name.empty() ? std::string("custom") : std::move(type_name);
            config->custom_parser = detail::make_typed_parser<T>(std::move(parser), config->custom_type_name);
            if (config->default_value.has_value()) {
                Command::validate_default_value(name_,
                                               config->kind,
                                               *config->default_value,
                                               "option",
                                               config->custom_type_name,
                                               true);
            }
        }
        return *this;
    }

    template <typename T, typename Parser, typename Formatter>
    OptionBuilder& parse_as(Parser parser, std::string type_name, Formatter formatter) {
        if (auto* config = config_mutable(); config != nullptr) {
            config->custom_type_name = type_name.empty() ? std::string("custom") : std::move(type_name);
            config->custom_parser =
                detail::make_typed_parser<T>(std::move(parser), std::move(formatter), config->custom_type_name);
            if (config->default_value.has_value()) {
                Command::validate_default_value(name_,
                                               config->kind,
                                               *config->default_value,
                                               "option",
                                               config->custom_type_name,
                                               true);
            }
        }
        return *this;
    }

private:
    [[nodiscard]] OptionConfig* config_mutable() const {
        return owner_ == nullptr ? nullptr : owner_->find_option_config_mutable(name_);
    }

    Command* owner_ = nullptr;
    std::string name_;
};

inline ArgumentBuilder Command::arg(std::string argument_name, ValueKind kind) {
    ArgumentConfig config;
    config.kind = kind;
    argument(argument_name, std::move(config));
    return ArgumentBuilder(this, std::move(argument_name));
}

inline OptionBuilder Command::opt(std::string option_name, ValueKind kind) {
    OptionConfig config;
    config.kind = kind;
    option(option_name, std::move(config));
    return OptionBuilder(this, std::move(option_name));
}

inline OptionGroup::OptionGroup(Command* owner, std::string name)
    : owner_(owner)
    , name_(std::move(name)) {}

inline OptionGroup& OptionGroup::description(std::string value) {
    if (owner_ != nullptr) {
        owner_->option_groups_[name_].description = std::move(value);
    }
    return *this;
}

inline OptionGroup& OptionGroup::option(std::string option_name, OptionConfig config) {
    if (owner_ != nullptr) {
        config.group = name_;
        owner_->option(std::move(option_name), std::move(config));
    }
    return *this;
}

}  // namespace clix
