#pragma once

#include <string>
#include <vector>

namespace clix {

/**
 * Structured metadata used to enrich command-related runtime errors.
 */
struct CommandErrorOptions {
    std::string command;
    std::string option;
    std::string argument;
    std::vector<std::string> expected;
    std::vector<std::string> received;
    std::vector<std::string> examples;
    std::string hint;
    bool auto_hint = false;
};

}  // namespace clix
