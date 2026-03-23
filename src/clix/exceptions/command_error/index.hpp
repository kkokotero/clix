#pragma once

#include <stdexcept>
#include <string>

#include "../parsing_error/utils/format.hpp"
#include "../parsing_error/utils/resolve_hint.hpp"
#include "types.hpp"

namespace clix {

/**
 * Error raised when the command configuration or command-line invocation is
 * semantically invalid.
 */
class CommandError : public std::runtime_error {
public:
    explicit CommandError(const std::string& message,
                          CommandErrorOptions options = {})
        : std::runtime_error(build_message(message, options)) {}

private:
    [[nodiscard]] static std::string build_message(const std::string& message,
                                                   const CommandErrorOptions& options) {
        std::string output = "Command Error\n" + message + '\n';

        if (!options.command.empty() || !options.option.empty() || !options.argument.empty()) {
            output += "\nContext:\n";

            if (!options.command.empty()) {
                output += "  Command: " + options.command + '\n';
            }

            if (!options.option.empty()) {
                output += "  Option: " + options.option + '\n';
            }

            if (!options.argument.empty()) {
                output += "  Argument: <" + options.argument + ">\n";
            }
        }

        if (!options.expected.empty()) {
            output += "\nExpected:\n  " + format_list(options.expected) + '\n';
        }

        if (!options.examples.empty()) {
            output += "\nExamples:\n  " + format_examples(options.examples) + '\n';
        }

        if (!options.received.empty()) {
            output += "\nReceived:\n  " + format_received(options.received) + '\n';
        }

        if (const auto hint =
                resolve_hint(options.hint, options.auto_hint, options.expected, options.received);
            !hint.empty()) {
            output += "\nHint:\n  " + hint + '\n';
        }

        while (!output.empty() && output.back() == '\n') {
            output.pop_back();
        }

        return output;
    }
};

}  // namespace clix
