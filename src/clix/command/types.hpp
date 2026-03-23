#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "../completion.hpp"
#include "../exceptions/command_error/index.hpp"
#include "../parser/value/types.hpp"
#include "../validators.hpp"

namespace clix {

class Command;
class OptionGroup;
class ArgumentBuilder;
class OptionBuilder;

using CustomValueParser = std::function<CliValue(std::string_view)>;

enum class ValueSource {
    command_line,
    config_file,
    environment,
    default_value,
};

[[nodiscard]] inline std::string to_string(ValueSource source) {
    switch (source) {
        case ValueSource::command_line:
            return "command_line";
        case ValueSource::config_file:
            return "config_file";
        case ValueSource::environment:
            return "environment";
        case ValueSource::default_value:
            return "default_value";
    }

    return "unknown";
}

/**
 * Positional argument definition.
 */
struct ArgumentConfig {
    ValueKind kind = ValueKind::string;
    std::string description;
    std::string value_label;
    bool optional = false;
    std::optional<CliValue> default_value;
    std::string custom_type_name;
    CustomValueParser custom_parser;
    std::vector<std::string> environment_variables;
    std::vector<std::string> choices;
    std::vector<CompletionSuggestion> completion_values;
    CompletionProvider completion_provider;
    std::vector<ValidatorSpec> validators;
};

/**
 * Option definition.
 */
struct OptionConfig {
    ValueKind kind = ValueKind::boolean;
    std::vector<std::string> aliases;
    std::string description;
    std::string value_label;
    std::string group = "Options";
    std::string exclusive_group;
    bool optional = true;
    std::optional<CliValue> default_value;
    std::string custom_type_name;
    CustomValueParser custom_parser;
    std::vector<std::string> environment_variables;
    std::vector<std::string> requires;
    std::vector<std::string> excludes;
    std::vector<std::string> choices;
    std::vector<CompletionSuggestion> completion_values;
    CompletionProvider completion_provider;
    std::vector<ValidatorSpec> validators;
    std::string deprecated_message;
    std::unordered_map<std::string, std::string> deprecated_alias_messages;
    bool hidden = false;
};

using ArgumentsMap = std::unordered_map<std::string, CliValue>;
using OptionsMap = std::unordered_map<std::string, CliValue>;
using CommandHandler = std::function<void(const class Invocation&)>;

struct OptionGroupDefinition {
    std::string name = "Options";
    std::string description;
};

class OptionGroup {
public:
    OptionGroup(Command* owner, std::string name);

    OptionGroup& description(std::string value);
    OptionGroup& option(std::string option_name, OptionConfig config = {});

private:
    Command* owner_ = nullptr;
    std::string name_;
};

/**
 * Immutable command invocation context delivered to action handlers.
 */
class Invocation {
public:
    Invocation(const Command* command,
               std::vector<std::string> command_path,
               ArgumentsMap arguments,
               OptionsMap options,
               std::unordered_map<std::string, ValueSource> argument_sources = {},
               std::unordered_map<std::string, ValueSource> option_sources = {},
               std::vector<std::string> passthrough_arguments = {})
        : command_(command)
        , command_path_(std::move(command_path))
        , arguments_(std::move(arguments))
        , options_(std::move(options))
        , argument_sources_(std::move(argument_sources))
        , option_sources_(std::move(option_sources))
        , passthrough_arguments_(std::move(passthrough_arguments)) {}

    [[nodiscard]] const Command& command() const { return *command_; }
    [[nodiscard]] const std::vector<std::string>& command_path() const noexcept { return command_path_; }
    [[nodiscard]] const ArgumentsMap& arguments() const noexcept { return arguments_; }
    [[nodiscard]] const OptionsMap& options() const noexcept { return options_; }
    [[nodiscard]] const std::vector<std::string>& passthrough_arguments() const noexcept {
        return passthrough_arguments_;
    }
    [[nodiscard]] const std::vector<std::string>& passthrough_tokens() const noexcept {
        return passthrough_arguments_;
    }
    [[nodiscard]] const std::unordered_map<std::string, ValueSource>& argument_sources() const noexcept {
        return argument_sources_;
    }
    [[nodiscard]] const std::unordered_map<std::string, ValueSource>& option_sources() const noexcept {
        return option_sources_;
    }

    [[nodiscard]] bool has_argument(std::string_view name) const {
        return arguments_.find(std::string(name)) != arguments_.end();
    }

    [[nodiscard]] bool has_option(std::string_view name) const {
        return options_.find(std::string(name)) != options_.end();
    }

    [[nodiscard]] const CliValue& argument_value(std::string_view name) const {
        const auto iterator = arguments_.find(std::string(name));
        if (iterator == arguments_.end()) {
            throw CommandError(
                "Requested argument is not available in this invocation",
                CommandErrorOptions{"", "", std::string(name)});
        }
        return iterator->second;
    }

    [[nodiscard]] const CliValue& option_value(std::string_view name) const {
        const auto iterator = options_.find(std::string(name));
        if (iterator == options_.end()) {
            throw CommandError(
                "Requested option is not available in this invocation",
                CommandErrorOptions{"", std::string(name), ""});
        }
        return iterator->second;
    }

    template <typename T>
    [[nodiscard]] const T& argument(std::string_view name) const {
        try {
            return value_cast<T>(argument_value(name));
        } catch (const std::bad_variant_access&) {
            throw CommandError(
                "Requested argument type does not match the parsed value",
                CommandErrorOptions{"", "", std::string(name)});
        }
    }

    template <typename T>
    [[nodiscard]] const T& option(std::string_view name) const {
        try {
            return value_cast<T>(option_value(name));
        } catch (const std::bad_variant_access&) {
            throw CommandError(
                "Requested option type does not match the parsed value",
                CommandErrorOptions{"", std::string(name), ""});
        }
    }

    template <typename T>
    [[nodiscard]] T option_or(std::string_view name, T fallback) const {
        const auto iterator = options_.find(std::string(name));
        if (iterator == options_.end()) {
            return fallback;
        }
        try {
            return value_cast<T>(iterator->second);
        } catch (const std::bad_variant_access&) {
            throw CommandError(
                "Requested option type does not match the parsed value",
                CommandErrorOptions{"", std::string(name), ""});
        }
    }

    [[nodiscard]] ValueSource argument_source(std::string_view name) const {
        const auto iterator = argument_sources_.find(std::string(name));
        if (iterator == argument_sources_.end()) {
            throw CommandError(
                "Requested argument source is not available in this invocation",
                CommandErrorOptions{"", "", std::string(name)});
        }
        return iterator->second;
    }

    [[nodiscard]] ValueSource option_source(std::string_view name) const {
        const auto iterator = option_sources_.find(std::string(name));
        if (iterator == option_sources_.end()) {
            throw CommandError(
                "Requested option source is not available in this invocation",
                CommandErrorOptions{"", std::string(name), ""});
        }
        return iterator->second;
    }

    [[nodiscard]] ValueSource source(std::string_view name) const {
        const auto argument_iterator = argument_sources_.find(std::string(name));
        const auto option_iterator = option_sources_.find(std::string(name));

        if (argument_iterator != argument_sources_.end() && option_iterator != option_sources_.end()) {
            throw CommandError(
                "Requested source is ambiguous",
                CommandErrorOptions{"",
                                    std::string(name),
                                    std::string(name),
                                    {},
                                    {},
                                    {},
                                    "Use argument_source() or option_source() when an argument and an option share the same name."});
        }

        if (argument_iterator != argument_sources_.end()) {
            return argument_iterator->second;
        }
        if (option_iterator != option_sources_.end()) {
            return option_iterator->second;
        }

        throw CommandError(
            "Requested source is not available in this invocation",
            CommandErrorOptions{"", std::string(name), std::string(name)});
    }

private:
    const Command* command_ = nullptr;
    std::vector<std::string> command_path_;
    ArgumentsMap arguments_;
    OptionsMap options_;
    std::unordered_map<std::string, ValueSource> argument_sources_;
    std::unordered_map<std::string, ValueSource> option_sources_;
    std::vector<std::string> passthrough_arguments_;
};

}  // namespace clix
