#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "utils/format.hpp"
#include "utils/resolve_hint.hpp"

namespace clix {

/**
 * Structured metadata used to enrich parsing failures with actionable output.
 */
struct ParsingErrorOptions {
    std::vector<std::string> expected;
    std::vector<std::string> received;
    std::vector<std::string> examples;
    std::string hint;
    bool auto_hint = false;
};

/**
 * Human-friendly parsing failure used across the library.
 */
class ParsingError : public std::runtime_error {
public:
    explicit ParsingError(const std::string& message,
                          ParsingErrorOptions options = {})
        : std::runtime_error(build_message(message, options)) {}

private:
    [[nodiscard]] static std::string build_message(const std::string& message,
                                                   const ParsingErrorOptions& options) {
        std::string output = "Parsing Error\n" + message + '\n';

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
