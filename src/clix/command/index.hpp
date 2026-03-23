#pragma once

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
    return config.value_label.empty() ? to_string(config.kind) : config.value_label;
}

inline std::string default_value_kind_label(const OptionConfig& config) {
    return config.value_label.empty() ? to_string(config.kind) : config.value_label;
}

inline std::string prefixed_option_name(std::string_view name) {
    return "--" + std::string(name);
}

inline std::string prefixed_alias_name(std::string_view name) {
    return "-" + std::string(name);
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

        if (std::find(aliases_.begin(), aliases_.end(), alias_name) != aliases_.end()) {
            throw CommandError(
                "Command alias is already defined",
                CommandErrorOptions{"", "", "", {}, {alias_name}, {}, "Use unique aliases per command."});
        }

        aliases_.push_back(std::move(alias_name));
        return *this;
    }

    Command& description(std::string description_text) {
        description_ = std::move(description_text);
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

    [[nodiscard]] bool allows_unknown_options() const noexcept { return allow_unknown_options_; }
    [[nodiscard]] bool allows_extra_arguments() const noexcept { return allow_extra_arguments_; }
    [[nodiscard]] bool allows_passthrough() const noexcept {
        return allow_unknown_options_ && allow_extra_arguments_;
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
            validate_default_value(argument_name, config.kind, *config.default_value, "argument");
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
            validate_default_value(option_name, config.kind, *config.default_value, "option");
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

    [[nodiscard]] const Command* find_subcommand(std::string_view token) const {
        const auto direct = commands_.find(std::string(token));
        if (direct != commands_.end()) {
            return direct->second.get();
        }

        for (const auto& [_, command_ptr] : commands_) {
            if (std::find(command_ptr->aliases_.begin(), command_ptr->aliases_.end(), token) !=
                command_ptr->aliases_.end()) {
                return command_ptr.get();
            }
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
    };

    [[nodiscard]] std::optional<OptionLookup> find_visible_option(std::string_view token) const {
        for (auto current = this; current != nullptr; current = current->parent_) {
            if (const auto option = current->find_option(token); option.has_value()) {
                return OptionLookup{current, option->first, option->second};
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
                    visible.push_back(OptionLookup{command, name, &config});
                } else {
                    *duplicate = OptionLookup{command, name, &config};
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
            suggestions.push_back({name, command_ptr->description_});
            for (const auto& alias_name : command_ptr->aliases_) {
                suggestions.push_back({alias_name, "alias for " + name});
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
                suggestions.push_back({detail::prefixed_alias_name(alias_name), option.config->description});
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

        auto value = coerce_argument(config->kind, raw, config->choices);
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

        auto value = coerce_option(config->kind, raw, config->choices);
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
            command_items.emplace_back(name + alias_text, command_ptr->description_);
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

    static void validate_default_value(const std::string& name,
                                       ValueKind kind,
                                       const CliValue& value,
                                       std::string_view label) {
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

    std::string name_;
    std::string description_;
    std::vector<std::string> aliases_;
    std::vector<std::pair<std::string, ArgumentConfig>> arguments_;
    std::vector<std::pair<std::string, OptionConfig>> options_;
    std::map<std::string, std::unique_ptr<Command>> commands_;
    std::map<std::string, OptionGroupDefinition> option_groups_;
    CommandHandler handler_;
    bool allow_unknown_options_ = false;
    bool allow_extra_arguments_ = false;
    Command* parent_ = nullptr;

    friend class OptionGroup;
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
            config->aliases.push_back(std::move(value));
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
