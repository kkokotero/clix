#pragma once

#include <string>
#include <vector>

namespace clix {

/**
 * Converts raw `argc` / `argv` input into the argument vector expected by the
 * CLI runtime.
 */
inline std::vector<std::string> normalize_arguments(int argc, const char* const* argv) {
    std::vector<std::string> arguments;
    arguments.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0U);

    for (int index = 1; index < argc; ++index) {
        arguments.emplace_back(argv[index] != nullptr ? argv[index] : "");
    }

    return arguments;
}

inline std::vector<std::string> normalize_arguments(int argc, char** argv) {
    return normalize_arguments(argc, const_cast<const char* const*>(argv));
}

inline std::vector<std::string> normalize_arguments(const std::vector<std::string>& args,
                                                    bool drop_executable = false) {
    if (!drop_executable || args.empty()) {
        return args;
    }

    return std::vector<std::string>(args.begin() + 1, args.end());
}

}  // namespace clix
