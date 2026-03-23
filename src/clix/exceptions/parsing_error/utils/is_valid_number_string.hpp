#pragma once

#include <regex>
#include <string_view>

namespace clix {

/**
 * Strict number validation for CLI text input.
 */
inline bool is_valid_number_string(std::string_view value) {
    static const std::regex pattern(R"(^-?\d+(\.\d+)?$)");
    return std::regex_match(value.begin(), value.end(), pattern);
}

}  // namespace clix
