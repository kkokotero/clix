#pragma once

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "command/index.hpp"
#include "completion.hpp"
#include "config.hpp"
#include "detail/strings.hpp"
#include "exceptions/command_error/index.hpp"
#include "parser/index.hpp"

namespace clix {

namespace detail {

inline std::string sanitize_symbol(std::string value) {
    for (auto& character : value) {
        if (!std::isalnum(static_cast<unsigned char>(character))) {
            character = '_';
        }
    }
    return value;
}

inline std::string completion_lines(const std::vector<CompletionSuggestion>& suggestions) {
    std::string result;
    for (std::size_t index = 0; index < suggestions.size(); ++index) {
        result += suggestions[index].value;
        if (!suggestions[index].description.empty()) {
            result += '\t';
            result += suggestions[index].description;
        }
        if (index + 1 < suggestions.size()) {
            result.push_back('\n');
        }
    }
    return result;
}

inline std::string render_template(
    std::string value,
    const std::vector<std::pair<std::string, std::string>>& replacements) {
    for (const auto& replacement : replacements) {
        value = replace_all_copy(std::move(value), replacement.first, replacement.second);
    }
    return value;
}

inline std::optional<std::string> read_process_environment(std::string_view name) {
    const auto* value = std::getenv(std::string(name).c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

}  // namespace detail

/**
 * Root CLI entry point.
 *
 * The class reuses the same builder API as a regular command so the root node
 * and subcommands behave consistently.
 */
class CLI : public Command {
public:
    using EnvironmentReader = std::function<std::optional<std::string>(std::string_view)>;

    explicit CLI(std::string name = {}, std::string version = "0.3.0")
        : Command(std::move(name))
        , version_(std::move(version)) {}

    CLI& version(std::string value) {
        version_ = std::move(value);
        return *this;
    }

    CLI& enable_config_files(ConfigFileSettings settings = {}) {
        settings.enabled = true;
        config_settings_ = std::move(settings);

        OptionConfig config_option;
        config_option.kind = ValueKind::path;
        config_option.aliases = config_settings_.aliases;
        config_option.description = config_settings_.description;
        config_option.value_label = "path";
        config_option.environment_variables = config_settings_.environment_variables;
        option(config_settings_.option_name, std::move(config_option));
        return *this;
    }

    CLI& environment_reader(EnvironmentReader reader) {
        environment_reader_ = std::move(reader);
        return *this;
    }

    CLI& enable_completion(CompletionSettings settings = {}) {
        settings.enabled = true;
        completion_settings_ = std::move(settings);

        OptionConfig completion_option;
        completion_option.kind = ValueKind::choice;
        completion_option.description = completion_settings_.description;
        completion_option.value_label = "shell";
        completion_option.choices = {"bash", "zsh", "fish", "powershell"};
        option(completion_settings_.generate_option, std::move(completion_option));
        return *this;
    }

    [[nodiscard]] const std::string& version() const noexcept { return version_; }
    [[nodiscard]] const ConfigFileSettings& config_file_settings() const noexcept { return config_settings_; }
    [[nodiscard]] const CompletionSettings& completion_settings() const noexcept { return completion_settings_; }

    [[nodiscard]] std::vector<CompletionSuggestion> complete(const std::vector<std::string>& words) const {
        auto prior = words;
        auto current = std::string();

        if (!prior.empty()) {
            current = prior.back();
            prior.pop_back();
        }

        const auto state = analyze_tokens(prior);

        if (state.pending_option_name.has_value()) {
            return state.active->complete_option_value(*state.pending_option_name, current);
        }

        auto parsed_current = parse_token(current);
        std::vector<CompletionSuggestion> suggestions;

        if (!state.end_of_flags && parsed_current.is_flag) {
            if (detail::starts_with(current, "--") && detail::contains(current, '=')) {
                const auto separator = current.find('=');
                const auto option_name = current.substr(2, separator - 2);
                const auto prefix = current.substr(separator + 1);
                return state.active->complete_option_value(option_name, prefix);
            }

            suggestions = state.active->complete_options(current);
            append_builtin_option_suggestions(suggestions, state, current);
            filter_completion_suggestions(suggestions, current);
            return suggestions;
        }

        if (!state.end_of_flags) {
            const auto subcommands = state.active->complete_subcommands(current);
            suggestions.insert(suggestions.end(), subcommands.begin(), subcommands.end());
        }

        if (state.positional_index < state.active->arguments_definitions().size()) {
            const auto positional = state.active->complete_argument(state.positional_index, current);
            suggestions.insert(suggestions.end(), positional.begin(), positional.end());
        }

        if (!state.end_of_flags && current.empty()) {
            const auto options = state.active->complete_options(current);
            suggestions.insert(suggestions.end(), options.begin(), options.end());
            append_builtin_option_suggestions(suggestions, state, current);
        }

        filter_completion_suggestions(suggestions, current);
        return suggestions;
    }

    [[nodiscard]] std::string completion_script(CompletionShell shell,
                                                std::string executable_name = {}) const {
        if (executable_name.empty()) {
            executable_name = name().empty() ? std::string("clix") : name();
        }

        const auto function_name = std::string("_clix_complete_") + detail::sanitize_symbol(executable_name);
        const auto hidden = completion_settings_.hidden_command.empty() ? "__clix_complete"
                                                                        : completion_settings_.hidden_command;
        const auto replacements = std::vector<std::pair<std::string, std::string>>{
            {"{{FUNCTION}}", function_name},
            {"{{EXECUTABLE}}", executable_name},
            {"{{HIDDEN}}", hidden},
        };

        switch (shell) {
            case CompletionShell::bash:
                return detail::render_template(
                    R"(# bash completion for {{EXECUTABLE}}
{{FUNCTION}}() {
    local IFS=$'\n'
    local lines=($("{{EXECUTABLE}}" {{HIDDEN}} bash "${COMP_WORDS[@]:1}" 2>/dev/null))
    COMPREPLY=()
    local line
    for line in "${lines[@]}"; do
        COMPREPLY+=("${line%%$'\t'*}")
    done
}
complete -F {{FUNCTION}} {{EXECUTABLE}}
)",
                    replacements);
            case CompletionShell::zsh:
                return detail::render_template(
                    R"(#compdef {{EXECUTABLE}}
{{FUNCTION}}() {
    local -a lines
    lines=("${(@f)$({{EXECUTABLE}} {{HIDDEN}} zsh "${words[@]:1}" 2>/dev/null)}")
    local -a completions
    local line value desc
    for line in $lines; do
        if [[ "$line" == *$'\t'* ]]; then
            value="${line%%$'\t'*}"
            desc="${line#*$'\t'}"
            completions+=("$value:$desc")
        else
            completions+=("$line")
        fi
    done
    _describe 'values' completions
}
{{FUNCTION}} "$words[1]"
)",
                    replacements);
            case CompletionShell::fish:
                return detail::render_template(
                    R"(function {{FUNCTION}}
    set -l words (commandline -opc)
    set -e words[1]
    for line in ({{EXECUTABLE}} {{HIDDEN}} fish $words (commandline -ct) 2>/dev/null)
        echo $line
    end
end
complete -c {{EXECUTABLE}} -f -a '({{FUNCTION}})'
)",
                    replacements);
            case CompletionShell::powershell:
                return detail::render_template(
                    R"(Register-ArgumentCompleter -Native -CommandName '{{EXECUTABLE}}' -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)
    $arguments = @()
    foreach ($element in $commandAst.CommandElements | Select-Object -Skip 1) {
        $arguments += $element.Extent.Text
    }

    $lines = & '{{EXECUTABLE}}' '{{HIDDEN}}' powershell @arguments 2>$null
    foreach ($line in $lines) {
        $parts = $line -split "`t", 2
        $value = $parts[0]
        $help = if ($parts.Length -gt 1) { $parts[1] } else { $parts[0] }
        [System.Management.Automation.CompletionResult]::new($value, $value, 'ParameterValue', $help)
    }
})
)",
                    replacements);
        }

        return {};
    }

    int run(int argc, const char* const* argv, std::ostream& out = std::cout, std::ostream& err = std::cerr) const {
        return run(normalize_arguments(argc, argv), out, err);
    }

    int run(int argc, char** argv, std::ostream& out = std::cout, std::ostream& err = std::cerr) const {
        return run(normalize_arguments(argc, argv), out, err);
    }

    int run(const std::vector<std::string>& argv,
            std::ostream& out = std::cout,
            std::ostream& err = std::cerr) const {
        try {
            auto tokens = argv;

            if (const auto hidden_completion = handle_hidden_completion(tokens, out); hidden_completion.has_value()) {
                return *hidden_completion;
            }

            if (const auto script = handle_completion_generation(tokens, out); script.has_value()) {
                return *script;
            }

            const auto config_document = load_config_document(tokens);
            auto state = initial_parse_state();
            auto arguments = ArgumentsMap{};
            auto options = OptionsMap{};
            auto argument_sources = std::unordered_map<std::string, ValueSource>{};
            auto option_sources = std::unordered_map<std::string, ValueSource>{};
            auto passthrough_arguments = std::vector<std::string>{};
            auto warnings = std::vector<std::string>{};

            parse_active_command_tokens(
                state, tokens, arguments, options, argument_sources, option_sources, passthrough_arguments, warnings);
            const auto command_line_arguments = collect_keys(arguments);
            const auto command_line_options = collect_keys(options);

            if (state.active != this && state.active->is_deprecated()) {
                warnings.push_back("Deprecated command `" + detail::join_strings(state.command_path, " ") + "`: " +
                                   state.active->deprecated_message());
            }

            apply_environment_values(*state.active, arguments, options, argument_sources, option_sources);

            if (config_document.has_value()) {
                apply_config_values(*state.active,
                                    state.command_path,
                                    *config_document,
                                    arguments,
                                    options,
                                    argument_sources,
                                    option_sources,
                                    command_line_arguments,
                                    command_line_options);
            }

            finalize_values(*state.active, arguments, options, argument_sources, option_sources);
            validate_option_relationships(*state.active, options);
            collect_deprecation_warnings(*state.active, options, option_sources, warnings);

            emit_warnings(warnings, err);

            if (!state.active->handler()) {
                out << state.active->help(state.command_path) << '\n';
                return 0;
            }

            state.active->handler()(Invocation(state.active,
                                               state.command_path,
                                               std::move(arguments),
                                               std::move(options),
                                               std::move(argument_sources),
                                               std::move(option_sources),
                                               std::move(passthrough_arguments)));
            return 0;
        } catch (const FlowControl& flow) {
            emit_warnings(flow.warnings, err);
            if (!flow.output.empty()) {
                (flow.to_stderr ? err : out) << flow.output << '\n';
            }
            return flow.exit_code;
        } catch (const std::exception& exception) {
            err << exception.what() << '\n';
            return 1;
        }
    }

private:
    struct FlowControl {
        int exit_code = 0;
        std::string output;
        bool to_stderr = false;
        std::vector<std::string> warnings;
    };

    struct ParseState {
        const Command* active = nullptr;
        std::vector<std::string> command_path;
        std::size_t subcommand_token_count = 0;
        std::size_t positional_index = 0;
        bool end_of_flags = false;
        std::optional<std::string> pending_option_name;
        std::set<std::string> seen_options;
    };

    [[nodiscard]] ParseState initial_parse_state() const {
        ParseState state;
        state.active = this;
        state.command_path = {name().empty() ? std::string("clix") : name()};
        return state;
    }

    [[nodiscard]] ParseState analyze_tokens(const std::vector<std::string>& tokens) const {
        auto state = initial_parse_state();

        for (std::size_t token_index = 0; token_index < tokens.size(); ++token_index) {
            const auto& token = tokens[token_index];
            const auto parsed = parse_token(token);

            if (parsed.is_end_of_flags) {
                state.end_of_flags = true;
                continue;
            }

            if (!state.end_of_flags && !parsed.is_flag && state.positional_index == 0) {
                if (const auto* subcommand = state.active->find_subcommand(token); subcommand != nullptr) {
                    state.command_path.push_back(subcommand->name());
                    state.active = subcommand;
                    state.subcommand_token_count = token_index + 1;
                    state.positional_index = 0;
                    continue;
                }
            }

            if (parsed.is_flag && !state.end_of_flags) {
                if (parsed.is_grouped && !parsed.value.has_value()) {
                    std::optional<std::string> value_option_name;
                    for (std::size_t alias_index = 0; alias_index < parsed.key.size(); ++alias_index) {
                        const auto alias_name = parsed.key[alias_index];
                        const auto option = state.active->find_visible_option(std::string(1, alias_name));
                        if (!option.has_value()) {
                            value_option_name.reset();
                            break;
                        }

                        state.seen_options.insert(option->name);
                        if (option->config->kind != ValueKind::boolean) {
                            if (alias_index + 1 < parsed.key.size()) {
                                value_option_name.reset();
                            } else {
                                value_option_name = option->name;
                            }
                            break;
                        }
                    }

                    if (value_option_name.has_value()) {
                        if (token_index + 1 >= tokens.size()) {
                            state.pending_option_name = *value_option_name;
                            break;
                        }

                        ++token_index;
                    }
                    continue;
                }

                const auto option = state.active->find_visible_option(parsed.key);
                if (!option.has_value()) {
                    continue;
                }

                state.seen_options.insert(option->name);

                if (option->config->kind == ValueKind::boolean || parsed.value.has_value()) {
                    continue;
                }

                if (token_index + 1 >= tokens.size()) {
                    state.pending_option_name = option->name;
                    break;
                }

                ++token_index;
                continue;
            }

            if (state.positional_index < state.active->arguments_definitions().size()) {
                ++state.positional_index;
            }
        }

        return state;
    }

    void parse_active_command_tokens(ParseState& state,
                                     const std::vector<std::string>& tokens,
                                     ArgumentsMap& arguments,
                                     OptionsMap& options,
                                     std::unordered_map<std::string, ValueSource>& argument_sources,
                                     std::unordered_map<std::string, ValueSource>& option_sources,
                                     std::vector<std::string>& passthrough_arguments,
                                     std::vector<std::string>& warnings) const {
        auto runtime_state = initial_parse_state();

        for (std::size_t index = 0; index < tokens.size(); ++index) {
            const auto& raw = tokens[index];
            auto parsed = parse_token(raw);
            const auto& positional_defs = runtime_state.active->arguments_definitions();

            if (parsed.is_end_of_flags) {
                if (index + 1 >= tokens.size()) {
                    throw CommandError(
                        "Unexpected end-of-options marker",
                        CommandErrorOptions{runtime_state.active->command_path_string(),
                                            "",
                                            "",
                                            {},
                                            {"--"},
                                            {},
                                            "Use `--` only when you need to force the remaining values to be positional."});
                }
                runtime_state.end_of_flags = true;
                continue;
            }

            if (parsed.is_flag && !runtime_state.end_of_flags && (parsed.key == "help" || parsed.key == "h")) {
                throw FlowControl{0, runtime_state.active->help(runtime_state.command_path), false, warnings};
            }

            if (parsed.is_flag && !runtime_state.end_of_flags && (parsed.key == "version" || parsed.key == "v")) {
                throw FlowControl{
                    0,
                    std::string(name().empty() ? "clix" : name()) + " v" + version_,
                    false,
                    warnings};
            }

            if (!runtime_state.end_of_flags && !parsed.is_flag && runtime_state.positional_index == 0) {
                if (const auto subcommand = runtime_state.active->find_subcommand_lookup(raw); subcommand.has_value()) {
                    runtime_state.command_path.push_back(subcommand->command->name());
                    runtime_state.active = subcommand->command;
                    runtime_state.subcommand_token_count = index + 1;
                    runtime_state.positional_index = 0;
                    if (subcommand->used_alias && subcommand->deprecated_alias_message.has_value()) {
                        warnings.push_back("Deprecated command alias `" + subcommand->matched_token + "` for `" +
                                           subcommand->command->name() + "`: " +
                                           *subcommand->deprecated_alias_message);
                    }
                    continue;
                }
            }

            if (!parsed.is_flag && !runtime_state.end_of_flags && positional_defs.empty() &&
                !runtime_state.active->commands().empty() && runtime_state.positional_index == 0) {
                std::vector<std::string> expected_commands;
                for (const auto& entry : runtime_state.active->commands()) {
                    expected_commands.push_back(entry.first);
                }

                throw CommandError(
                    "Unknown command: " + raw,
                    CommandErrorOptions{runtime_state.active->command_path_string(),
                                        "",
                                        raw,
                                        expected_commands,
                                        {raw},
                                        {},
                                        "",
                                        true});
            }

            if (parsed.is_flag && !runtime_state.end_of_flags) {
                const auto visible_options = runtime_state.active->visible_options();

                if (parsed.is_grouped && !parsed.value.has_value()) {
                    for (std::size_t alias_index = 0; alias_index < parsed.key.size(); ++alias_index) {
                        const auto alias_name = std::string(1, parsed.key[alias_index]);
                        const auto option = runtime_state.active->find_visible_option(alias_name);
                        if (!option.has_value()) {
                            if (runtime_state.active->allows_unknown_options()) {
                                passthrough_arguments.push_back("-" + alias_name);
                                continue;
                            }

                            std::vector<std::string> expected_options;
                            for (const auto& visible_option : visible_options) {
                                expected_options.push_back(detail::prefixed_option_name(visible_option.name));
                                for (const auto& alias : visible_option.config->aliases) {
                                    expected_options.push_back(detail::prefixed_alias_name(alias));
                                }
                            }

                            throw CommandError(
                                "Unknown option: -" + alias_name,
                                CommandErrorOptions{runtime_state.active->command_path_string(),
                                                    "-" + alias_name,
                                                    "",
                                                    expected_options,
                                                    {"-" + alias_name},
                                                    {},
                                                    "",
                                                    true});
                        }

                        const auto& option_name = option->name;
                        const auto& option_config = *option->config;
                        if (option_config.kind == ValueKind::boolean) {
                            const auto value = CliValue(true);
                            runtime_state.active->validate_option_value(option_name, value);
                            options[option_name] = value;
                            option_sources[option_name] = ValueSource::command_line;
                            if (!option_config.deprecated_message.empty()) {
                                warnings.push_back("Deprecated option `" +
                                                   detail::prefixed_option_name(option_name) + "`: " +
                                                   option_config.deprecated_message);
                            }
                            if (option->used_alias && option->deprecated_alias_message.has_value()) {
                                warnings.push_back("Deprecated option alias `" +
                                                   detail::prefixed_alias_name(option->matched_token) + "` for `" +
                                                   detail::prefixed_option_name(option_name) + "`: " +
                                                   *option->deprecated_alias_message);
                            }
                            continue;
                        }

                        if (alias_index + 1 < parsed.key.size()) {
                            options[option_name] = runtime_state.active->parse_option_value(
                                option_name,
                                parsed.key.substr(alias_index + 1));
                            option_sources[option_name] = ValueSource::command_line;
                            if (!option_config.deprecated_message.empty()) {
                                warnings.push_back("Deprecated option `" +
                                                   detail::prefixed_option_name(option_name) + "`: " +
                                                   option_config.deprecated_message);
                            }
                            if (option->used_alias && option->deprecated_alias_message.has_value()) {
                                warnings.push_back("Deprecated option alias `" +
                                                   detail::prefixed_alias_name(option->matched_token) + "` for `" +
                                                   detail::prefixed_option_name(option_name) + "`: " +
                                                   *option->deprecated_alias_message);
                            }
                            break;
                        }

                        if (index + 1 >= tokens.size()) {
                            throw CommandError(
                                "Option \"" + detail::prefixed_option_name(option_name) + "\" expects a value",
                                CommandErrorOptions{runtime_state.active->command_path_string(),
                                                    option_name,
                                                    "",
                                                    {to_string(option_config.kind)},
                                                    {},
                                                    {},
                                                    "Provide a value immediately after the option."});
                        }

                        options[option_name] = runtime_state.active->parse_option_value(option_name, tokens[index + 1]);
                        option_sources[option_name] = ValueSource::command_line;
                        if (!option_config.deprecated_message.empty()) {
                            warnings.push_back("Deprecated option `" +
                                               detail::prefixed_option_name(option_name) + "`: " +
                                               option_config.deprecated_message);
                        }
                        if (option->used_alias && option->deprecated_alias_message.has_value()) {
                            warnings.push_back("Deprecated option alias `" +
                                               detail::prefixed_alias_name(option->matched_token) + "` for `" +
                                               detail::prefixed_option_name(option_name) + "`: " +
                                               *option->deprecated_alias_message);
                        }
                        ++index;
                    }
                    continue;
                }

                if (visible_options.empty() && !runtime_state.active->allows_unknown_options()) {
                    throw CommandError(
                        "Unknown option: " + parsed.original,
                        CommandErrorOptions{runtime_state.active->command_path_string(),
                                            parsed.original,
                                            "",
                                            {},
                                            {},
                                            {},
                                            "This command does not accept any options."});
                }

                const auto option = runtime_state.active->find_visible_option(parsed.key);
                if (!option.has_value()) {
                    if (runtime_state.active->allows_unknown_options()) {
                        passthrough_arguments.push_back(raw);
                        if (!parsed.value.has_value() && index + 1 < tokens.size()) {
                            const auto next = parse_token(tokens[index + 1]);
                            if (!next.is_flag && !next.is_end_of_flags) {
                                passthrough_arguments.push_back(tokens[index + 1]);
                                ++index;
                            }
                        }
                        continue;
                    }

                    std::vector<std::string> expected_options;
                    for (const auto& visible_option : visible_options) {
                        expected_options.push_back(detail::prefixed_option_name(visible_option.name));
                        for (const auto& alias_name : visible_option.config->aliases) {
                            expected_options.push_back(detail::prefixed_alias_name(alias_name));
                        }
                    }

                    throw CommandError(
                        "Unknown option: " + parsed.original,
                        CommandErrorOptions{runtime_state.active->command_path_string(),
                                            parsed.original,
                                            "",
                                            expected_options,
                                            {parsed.original},
                                            {},
                                            "",
                                            true});
                }

                const auto& option_name = option->name;
                const auto& option_config = *option->config;

                if (option_config.kind == ValueKind::boolean) {
                    if (parsed.value.has_value()) {
                        auto value = runtime_state.active->parse_option_value(option_name, *parsed.value);
                        options[option_name] = value;
                    } else {
                        const auto value = CliValue(!parsed.is_negation);
                        runtime_state.active->validate_option_value(option_name, value);
                        options[option_name] = value;
                    }
                    option_sources[option_name] = ValueSource::command_line;
                    if (!option_config.deprecated_message.empty()) {
                        warnings.push_back("Deprecated option `" +
                                           detail::prefixed_option_name(option_name) + "`: " +
                                           option_config.deprecated_message);
                    }
                    if (option->used_alias && option->deprecated_alias_message.has_value()) {
                        warnings.push_back("Deprecated option alias `" +
                                           detail::prefixed_alias_name(option->matched_token) + "` for `" +
                                           detail::prefixed_option_name(option_name) + "`: " +
                                           *option->deprecated_alias_message);
                    }
                    continue;
                }

                if (parsed.is_negation) {
                    throw CommandError(
                        "Boolean negation is not supported for " + detail::prefixed_option_name(option_name),
                        CommandErrorOptions{runtime_state.active->command_path_string(),
                                            option_name,
                                            "",
                                            {},
                                            {},
                                            {},
                                            "Only boolean options can use the --no-<name> syntax."});
                }

                if (parsed.value.has_value()) {
                    options[option_name] = runtime_state.active->parse_option_value(option_name, *parsed.value);
                    option_sources[option_name] = ValueSource::command_line;
                    if (!option_config.deprecated_message.empty()) {
                        warnings.push_back("Deprecated option `" +
                                           detail::prefixed_option_name(option_name) + "`: " +
                                           option_config.deprecated_message);
                    }
                    if (option->used_alias && option->deprecated_alias_message.has_value()) {
                        warnings.push_back("Deprecated option alias `" +
                                           detail::prefixed_alias_name(option->matched_token) + "` for `" +
                                           detail::prefixed_option_name(option_name) + "`: " +
                                           *option->deprecated_alias_message);
                    }
                    continue;
                }

                if (index + 1 >= tokens.size()) {
                    throw CommandError(
                        "Option \"" + detail::prefixed_option_name(option_name) + "\" expects a value",
                        CommandErrorOptions{runtime_state.active->command_path_string(),
                                            option_name,
                                            "",
                                            {to_string(option_config.kind)},
                                            {},
                                            {},
                                            "Provide a value immediately after the option."});
                }

                options[option_name] = runtime_state.active->parse_option_value(option_name, tokens[index + 1]);
                option_sources[option_name] = ValueSource::command_line;
                if (!option_config.deprecated_message.empty()) {
                    warnings.push_back("Deprecated option `" + detail::prefixed_option_name(option_name) + "`: " +
                                       option_config.deprecated_message);
                }
                if (option->used_alias && option->deprecated_alias_message.has_value()) {
                    warnings.push_back("Deprecated option alias `" +
                                       detail::prefixed_alias_name(option->matched_token) + "` for `" +
                                       detail::prefixed_option_name(option_name) + "`: " +
                                       *option->deprecated_alias_message);
                }
                ++index;
                continue;
            }

            const auto has_pending_positional = runtime_state.positional_index < positional_defs.size();
            if (has_pending_positional) {
                const auto& argument_name = positional_defs[runtime_state.positional_index].first;
                arguments[argument_name] = runtime_state.active->parse_argument_value(argument_name, raw);
                argument_sources[argument_name] = ValueSource::command_line;
                ++runtime_state.positional_index;
                continue;
            }

            if (runtime_state.active->allows_extra_arguments()) {
                passthrough_arguments.push_back(raw);
                continue;
            }

            if (runtime_state.active->arguments_definitions().empty()) {
                throw CommandError(
                    "Unexpected argument: " + raw,
                    CommandErrorOptions{runtime_state.active->command_path_string(),
                                        "",
                                        raw,
                                        {},
                                        {},
                                        {},
                                        "This command does not accept positional arguments."});
            }

            throw CommandError(
                "Unexpected argument: " + raw,
                CommandErrorOptions{runtime_state.active->command_path_string(),
                                    "",
                                    raw,
                                    {},
                                    {},
                                    {},
                                    "No more positional arguments are defined for this command."});
        }

        state = runtime_state;
    }

    void finalize_values(const Command& command,
                         ArgumentsMap& arguments,
                         OptionsMap& options,
                         std::unordered_map<std::string, ValueSource>& argument_sources,
                         std::unordered_map<std::string, ValueSource>& option_sources) const {
        for (const auto& [argument_name, argument_config] : command.arguments_definitions()) {
            if (detail::contains_key(arguments, argument_name)) {
                continue;
            }

            if (argument_config.default_value.has_value()) {
                command.validate_argument_value(argument_name, *argument_config.default_value);
                arguments.emplace(argument_name, *argument_config.default_value);
                argument_sources.insert_or_assign(argument_name, ValueSource::default_value);
                continue;
            }

            if (!argument_config.optional) {
                throw CommandError(
                    "Missing required argument: <" + argument_name + ">",
                    CommandErrorOptions{command.command_path_string(),
                                        "",
                                        argument_name,
                                        {},
                                        {},
                                        {},
                                        "This argument is required."});
            }
        }

        for (const auto& option : command.visible_options()) {
            const auto& option_name = option.name;
            const auto& option_config = *option.config;

            if (detail::contains_key(options, option_name)) {
                continue;
            }

            if (option_config.default_value.has_value()) {
                command.validate_option_value(option_name, *option_config.default_value);
                options.emplace(option_name, *option_config.default_value);
                option_sources.insert_or_assign(option_name, ValueSource::default_value);
                continue;
            }

            if (option_config.kind == ValueKind::boolean) {
                const auto value = CliValue(false);
                command.validate_option_value(option_name, value);
                options.emplace(option_name, value);
                option_sources.insert_or_assign(option_name, ValueSource::default_value);
                continue;
            }

            if (!option_config.optional) {
                throw CommandError(
                    "Missing required option: " + detail::prefixed_option_name(option_name),
                    CommandErrorOptions{command.command_path_string(),
                                        option_name,
                                        "",
                                        {},
                                        {},
                                        {},
                                        "This option is required."});
            }
        }
    }

    void apply_environment_values(const Command& command,
                                  ArgumentsMap& arguments,
                                  OptionsMap& options,
                                  std::unordered_map<std::string, ValueSource>& argument_sources,
                                  std::unordered_map<std::string, ValueSource>& option_sources) const {
        for (const auto& entry : command.arguments_definitions()) {
            const auto& argument_name = entry.first;
            const auto& argument_config = entry.second;

            if (detail::contains_key(arguments, argument_name)) {
                continue;
            }

            for (const auto& variable_name : argument_config.environment_variables) {
                const auto value = read_environment(variable_name);
                if (!value.has_value() || detail::trim_copy(*value).empty()) {
                    continue;
                }

                arguments.emplace(argument_name, command.parse_argument_value(argument_name, *value));
                argument_sources.insert_or_assign(argument_name, ValueSource::environment);
                break;
            }
        }

        for (const auto& option : command.visible_options()) {
            const auto& option_name = option.name;
            const auto& option_config = *option.config;

            if (detail::contains_key(options, option_name)) {
                continue;
            }

            for (const auto& variable_name : option_config.environment_variables) {
                const auto value = read_environment(variable_name);
                if (!value.has_value() || detail::trim_copy(*value).empty()) {
                    continue;
                }

                options.emplace(option_name, command.parse_option_value(option_name, *value));
                option_sources.insert_or_assign(option_name, ValueSource::environment);
                break;
            }
        }
    }

    [[nodiscard]] bool option_is_active(const Command& command,
                                        const OptionsMap& options,
                                        std::string_view option_name) const {
        const auto iterator = options.find(std::string(option_name));
        if (iterator == options.end()) {
            return false;
        }

        const auto* config = command.find_visible_option_config(option_name);
        if (config == nullptr) {
            return false;
        }

        if (config->kind == ValueKind::boolean) {
            return value_cast<bool>(iterator->second);
        }

        return true;
    }

    void collect_deprecation_warnings(const Command& command,
                                      const OptionsMap& options,
                                      const std::unordered_map<std::string, ValueSource>& option_sources,
                                      std::vector<std::string>& warnings) const {
        for (const auto& option : command.visible_options()) {
            const auto& option_name = option.name;
            const auto& option_config = *option.config;

            if (option_config.deprecated_message.empty()) {
                continue;
            }

            if (!option_is_active(command, options, option_name)) {
                continue;
            }

            const auto source = option_sources.find(option_name);
            if (source == option_sources.end() || source->second == ValueSource::default_value) {
                continue;
            }

            warnings.push_back("Deprecated option `" + detail::prefixed_option_name(option_name) + "`: " +
                               option_config.deprecated_message);
        }
    }

    static void emit_warnings(const std::vector<std::string>& warnings, std::ostream& err) {
        std::unordered_set<std::string> seen;
        for (const auto& warning : warnings) {
            if (warning.empty() || !seen.insert(warning).second) {
                continue;
            }
            err << "warning: " << warning << '\n';
        }
    }

    void validate_option_relationships(const Command& command,
                                       const OptionsMap& options) const {
        std::unordered_map<std::string, std::string> active_exclusive_groups;

        for (const auto& option : command.visible_options()) {
            const auto& option_name = option.name;
            const auto& option_config = *option.config;

            if (!option_is_active(command, options, option_name)) {
                continue;
            }

            if (!option_config.exclusive_group.empty()) {
                const auto existing = active_exclusive_groups.find(option_config.exclusive_group);
                if (existing != active_exclusive_groups.end()) {
                    throw CommandError(
                        "Mutually exclusive options",
                        CommandErrorOptions{command.command_path_string(),
                                            option_name,
                                            "",
                                            {},
                                            {detail::prefixed_option_name(existing->second),
                                             detail::prefixed_option_name(option_name)},
                                            {},
                                            "Only one option from the \"" + option_config.exclusive_group +
                                                "\" exclusive group can be active."});
                }

                active_exclusive_groups.emplace(option_config.exclusive_group, option_name);
            }

            for (const auto& required_option : option_config.requires) {
                if (option_is_active(command, options, required_option)) {
                    continue;
                }

                throw CommandError(
                    "Missing required related option",
                    CommandErrorOptions{command.command_path_string(),
                                        option_name,
                                        "",
                                        {detail::prefixed_option_name(required_option)},
                                        {detail::prefixed_option_name(option_name)},
                                        {},
                                        detail::prefixed_option_name(option_name) + " requires " +
                                            detail::prefixed_option_name(required_option) + "."});
            }

            for (const auto& excluded_option : option_config.excludes) {
                if (!option_is_active(command, options, excluded_option)) {
                    continue;
                }

                throw CommandError(
                    "Conflicting options",
                    CommandErrorOptions{command.command_path_string(),
                                        option_name,
                                        "",
                                        {},
                                        {detail::prefixed_option_name(option_name),
                                         detail::prefixed_option_name(excluded_option)},
                                        {},
                                        detail::prefixed_option_name(option_name) + " cannot be used together with " +
                                            detail::prefixed_option_name(excluded_option) + "."});
            }
        }
    }

    [[nodiscard]] std::optional<int> handle_hidden_completion(const std::vector<std::string>& tokens,
                                                              std::ostream& out) const {
        if (!completion_settings_.enabled || tokens.empty() ||
            tokens.front() != completion_settings_.hidden_command) {
            return std::nullopt;
        }

        const std::vector<std::string> words(tokens.begin() + 2, tokens.end());
        out << detail::completion_lines(complete(words));
        return 0;
    }

    [[nodiscard]] std::optional<int> handle_completion_generation(std::vector<std::string>& tokens,
                                                                  std::ostream& out) const {
        if (!completion_settings_.enabled) {
            return std::nullopt;
        }

        if (const auto shell = extract_special_option_value(tokens,
                                                            completion_settings_.generate_option,
                                                            {}); shell.has_value()) {
            const auto parsed_shell = completion_shell_from_string(*shell);
            if (!parsed_shell.has_value()) {
                throw CommandError(
                    "Unsupported completion shell",
                    CommandErrorOptions{command_path_string(),
                                        completion_settings_.generate_option,
                                        "",
                                        {"bash", "zsh", "fish", "powershell"},
                                        {*shell},
                                        {},
                                        "Use one of the supported shell identifiers."});
            }

            out << completion_script(*parsed_shell, name().empty() ? "clix" : name()) << '\n';
            return 0;
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<ConfigDocument> load_config_document(std::vector<std::string>& tokens) const {
        if (!config_settings_.enabled) {
            return std::nullopt;
        }

        const auto value = resolve_config_path(tokens);
        if (!value.has_value()) {
            return std::nullopt;
        }

        return ConfigDocument::parse_file(Path(*value), config_settings_);
    }

    void apply_config_values(const Command& command,
                             const std::vector<std::string>& command_path,
                             const ConfigDocument& document,
                             ArgumentsMap& arguments,
                             OptionsMap& options,
                             std::unordered_map<std::string, ValueSource>& argument_sources,
                             std::unordered_map<std::string, ValueSource>& option_sources,
                             const std::unordered_set<std::string>& protected_arguments,
                             const std::unordered_set<std::string>& protected_options) const {
        std::unordered_map<std::string, std::string> merged_values;

        for (const auto& section : document.resolve_command_sections(command_path)) {
            for (const auto& [key, value] : section) {
                merged_values[key] = value;
            }
        }

        if (config_settings_.strict) {
            for (const auto& [key, _] : merged_values) {
                if (command.find_argument(key) == nullptr && command.find_visible_option_config(key) == nullptr) {
                    throw CommandError(
                        "Unknown config key",
                        CommandErrorOptions{command.command_path_string(),
                                            "",
                                            key,
                                            {},
                                            {},
                                            {},
                                            "Every config key must match a declared argument or option."});
                }
            }
        }

        for (const auto& [argument_name, _] : command.arguments_definitions()) {
            if (detail::contains_key(protected_arguments, argument_name)) {
                continue;
            }

            const auto iterator = merged_values.find(argument_name);
            if (iterator != merged_values.end()) {
                arguments.insert_or_assign(argument_name,
                                           command.parse_argument_value(argument_name, iterator->second));
                argument_sources.insert_or_assign(argument_name, ValueSource::config_file);
            }
        }

        for (const auto& option : command.visible_options()) {
            const auto& option_name = option.name;
            if (detail::contains_key(protected_options, option_name)) {
                continue;
            }

            const auto iterator = merged_values.find(option_name);
            if (iterator != merged_values.end()) {
                options.insert_or_assign(option_name,
                                         command.parse_option_value(option_name, iterator->second));
                option_sources.insert_or_assign(option_name, ValueSource::config_file);
            }
        }
    }

    void append_builtin_option_suggestions(std::vector<CompletionSuggestion>& suggestions,
                                           const ParseState& state,
                                           std::string_view prefix) const {
        suggestions.push_back({"--help", "Show help for this command."});
        suggestions.push_back({"-h", "Show help for this command."});
        suggestions.push_back({"--version", "Show the CLI version."});
        suggestions.push_back({"-v", "Show the CLI version."});

        if (completion_settings_.enabled && state.active == this) {
            suggestions.push_back({detail::prefixed_option_name(completion_settings_.generate_option),
                                   completion_settings_.description});
        }

        if (config_settings_.enabled && state.active == this) {
            suggestions.push_back({detail::prefixed_option_name(config_settings_.option_name),
                                   config_settings_.description});
            for (const auto& alias_name : config_settings_.aliases) {
                suggestions.push_back({detail::prefixed_alias_name(alias_name), config_settings_.description});
            }
        }

        filter_completion_suggestions(suggestions, prefix);
    }

    [[nodiscard]] std::optional<std::string> extract_special_option_value(std::vector<std::string>& tokens,
                                                                          const std::string& option_name,
                                                                          const std::vector<std::string>& aliases) const {
        std::optional<std::string> value;

        for (std::size_t index = 0; index < tokens.size();) {
            const auto parsed = parse_token(tokens[index]);
            const auto matches_long = parsed.is_flag && parsed.key == option_name;
            const auto matches_alias =
                parsed.is_flag &&
                std::find(aliases.begin(), aliases.end(), parsed.key) != aliases.end();

            if (!matches_long && !matches_alias) {
                ++index;
                continue;
            }

            if (parsed.value.has_value()) {
                value = *parsed.value;
                tokens.erase(tokens.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }

            if (index + 1 >= tokens.size()) {
                throw CommandError(
                    "Option \"" + detail::prefixed_option_name(option_name) + "\" expects a value",
                    CommandErrorOptions{command_path_string(),
                                        option_name,
                                        "",
                                        {},
                                        {},
                                        {},
                                        "Provide a value immediately after the special option."});
            }

            value = tokens[index + 1];
            tokens.erase(tokens.begin() + static_cast<std::ptrdiff_t>(index),
                         tokens.begin() + static_cast<std::ptrdiff_t>(index + 2));
        }

        return value;
    }

    [[nodiscard]] std::optional<std::string> resolve_config_path(std::vector<std::string>& tokens) const {
        if (const auto explicit_value =
                extract_special_option_value(tokens, config_settings_.option_name, config_settings_.aliases);
            explicit_value.has_value()) {
            return resolve_config_candidate_path(*explicit_value, true);
        }

        for (const auto& variable_name : config_settings_.environment_variables) {
            const auto value = read_environment(variable_name);
            if (!value.has_value() || detail::trim_copy(*value).empty()) {
                continue;
            }
            return resolve_config_candidate_path(*value, true);
        }

        for (const auto& filename : config_settings_.default_filenames) {
            if (const auto value = resolve_config_candidate_path(filename, false); value.has_value()) {
                return value;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::string> resolve_config_candidate_path(std::string_view candidate,
                                                                           bool required) const {
        const auto trimmed = detail::trim_copy(candidate);
        if (trimmed.empty()) {
            return std::nullopt;
        }

        const auto raw_path = std::filesystem::path(trimmed);
        auto probes = std::vector<std::filesystem::path>{};

        if (raw_path.extension().empty()) {
            for (const auto& extension : config_settings_.allowed_extensions) {
                auto probe = raw_path;
                probe += normalize_config_extension(extension);
                probes.push_back(std::move(probe));
            }
        } else {
            probes.push_back(raw_path);
        }

        for (const auto& probe : probes) {
            std::error_code error;
            if (std::filesystem::exists(probe, error) && !error) {
                return probe.string();
            }
        }

        if (!required) {
            return std::nullopt;
        }

        if (!raw_path.extension().empty()) {
            return raw_path.string();
        }

        if (!probes.empty()) {
            return probes.front().string();
        }

        return raw_path.string();
    }

    [[nodiscard]] static std::string normalize_config_extension(std::string_view extension) {
        if (extension.empty()) {
            return {};
        }
        return extension.front() == '.' ? std::string(extension) : "." + std::string(extension);
    }

    [[nodiscard]] std::optional<std::string> read_environment(std::string_view name) const {
        if (environment_reader_) {
            if (const auto custom_value = environment_reader_(name); custom_value.has_value()) {
                return custom_value;
            }
        }
        return detail::read_process_environment(name);
    }

    template <typename Map>
    [[nodiscard]] static std::unordered_set<std::string> collect_keys(const Map& map) {
        std::unordered_set<std::string> keys;
        keys.reserve(map.size());
        for (const auto& entry : map) {
            keys.insert(entry.first);
        }
        return keys;
    }

    std::string version_;
    ConfigFileSettings config_settings_;
    CompletionSettings completion_settings_;
    EnvironmentReader environment_reader_;
};

}  // namespace clix
