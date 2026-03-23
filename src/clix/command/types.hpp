#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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

/**
 * Positional argument definition.
 */
struct ArgumentConfig {
    ValueKind kind = ValueKind::string;
    std::string description;
    std::string value_label;
    bool optional = false;
    std::optional<CliValue> default_value;
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
    std::vector<std::string> environment_variables;
    std::vector<std::string> requires;
    std::vector<std::string> excludes;
    std::vector<std::string> choices;
    std::vector<CompletionSuggestion> completion_values;
    CompletionProvider completion_provider;
    std::vector<ValidatorSpec> validators;
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
               std::vector<std::string> passthrough_arguments = {})
        : command_(command)
        , command_path_(std::move(command_path))
        , arguments_(std::move(arguments))
        , options_(std::move(options))
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
        return std::get<T>(argument_value(name));
    }

    template <typename T>
    [[nodiscard]] const T& option(std::string_view name) const {
        return std::get<T>(option_value(name));
    }

    template <typename T>
    [[nodiscard]] T option_or(std::string_view name, T fallback) const {
        const auto iterator = options_.find(std::string(name));
        if (iterator == options_.end()) {
            return fallback;
        }
        return std::get<T>(iterator->second);
    }

private:
    const Command* command_ = nullptr;
    std::vector<std::string> command_path_;
    ArgumentsMap arguments_;
    OptionsMap options_;
    std::vector<std::string> passthrough_arguments_;
};

}  // namespace clix
