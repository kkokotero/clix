#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../../../detail/strings.hpp"
#include "../../../exceptions/parsing_error/index.hpp"

namespace clix {

inline std::vector<std::string> parse_list(std::string_view input) {
    auto text = std::string(input);

    const auto begin = text.find_first_not_of(" \t\n\r\f\v");
    if (begin == std::string::npos) {
        throw ParsingError(
            "Input string is empty",
            ParsingErrorOptions{{},
                                {std::string(input)},
                                {"a,b,c", "[1,2,3]", "apple, banana, cherry"},
                                ""});
    }

    const auto end = text.find_last_not_of(" \t\n\r\f\v");
    text = text.substr(begin, end - begin + 1);

    if (detail::starts_with(text, "[") && detail::ends_with(text, "]")) {
        text = text.substr(1, text.size() - 2);
    }

    std::vector<std::string> values;
    std::size_t cursor = 0;

    while (cursor <= text.size()) {
        const auto next = text.find(',', cursor);
        auto token = text.substr(cursor, next == std::string::npos ? std::string::npos : next - cursor);

        const auto token_begin = token.find_first_not_of(" \t\n\r\f\v");
        if (token_begin != std::string::npos) {
            const auto token_end = token.find_last_not_of(" \t\n\r\f\v");
            token = token.substr(token_begin, token_end - token_begin + 1);
            if (!token.empty()) {
                values.push_back(token);
            }
        }

        if (next == std::string::npos) {
            break;
        }

        cursor = next + 1;
    }

    if (values.empty()) {
        throw ParsingError(
            "Input list is empty",
            ParsingErrorOptions{{},
                                {std::string(input)},
                                {"a,b,c", "[1,2,3]", "apple, banana, cherry"},
                                ""});
    }

    return values;
}

}  // namespace clix
