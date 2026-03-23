#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "../../../detail/strings.hpp"
#include "../../../exceptions/parsing_error/index.hpp"

namespace clix {

inline std::string parse_choice(std::string_view input, const std::vector<std::string>& choices) {
    const auto trimmed = [&]() {
        const auto begin = input.find_first_not_of(" \t\n\r\f\v");
        if (begin == std::string_view::npos) {
            return std::string();
        }
        const auto end = input.find_last_not_of(" \t\n\r\f\v");
        return std::string(input.substr(begin, end - begin + 1));
    }();

    if (choices.empty()) {
        throw ParsingError(
            "Choice values are not configured",
            ParsingErrorOptions{{}, {}, {}, "Define at least one allowed choice for this argument or option."});
    }

    if (std::find(choices.begin(), choices.end(), trimmed) != choices.end()) {
        return trimmed;
    }

    throw ParsingError(
        "Invalid choice value",
        ParsingErrorOptions{choices,
                            {std::string(input)},
                            {},
                            "Valid choices are: " + detail::join_strings(choices, ", ")});
}

}  // namespace clix
