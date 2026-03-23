#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

#include "../../../exceptions/parsing_error/index.hpp"

namespace clix {

inline double parse_number(std::string_view input) {
    try {
        std::size_t consumed = 0;
        const auto value = std::stod(std::string(input), &consumed);
        if (consumed != std::string(input).size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (const std::exception&) {
        throw ParsingError(
            "Invalid number value",
            ParsingErrorOptions{{},
                                {std::string(input)},
                                {"42", "-3.14", "0"},
                                "Use a numeric value such as 0, 1, -1, or 3.14."});
    }
}

}  // namespace clix
