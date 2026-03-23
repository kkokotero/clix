#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "../../../exceptions/parsing_error/index.hpp"
#include "../constants.hpp"

namespace clix {

inline bool parse_boolean(std::string_view input) {
    std::string normalized(input);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });

    const auto begin = normalized.find_first_not_of(" \t\n\r\f\v");
    const auto end = normalized.find_last_not_of(" \t\n\r\f\v");
    normalized = begin == std::string::npos ? std::string() : normalized.substr(begin, end - begin + 1);

    for (const auto candidate : k_true_values) {
        if (normalized == candidate) {
            return true;
        }
    }

    for (const auto candidate : k_false_values) {
        if (normalized == candidate) {
            return false;
        }
    }

    throw ParsingError(
        "Invalid boolean value",
        ParsingErrorOptions{{"true", "false", "1", "0", "yes", "no", "on", "off"},
                            {std::string(input)},
                            {},
                            "",
                            true});
}

}  // namespace clix
