#pragma once

#include <string>
#include <string_view>

#include "../../detail/strings.hpp"
#include "constants.hpp"
#include "types.hpp"

namespace clix {

inline ParsedToken parse_token(std::string_view token) {
    ParsedToken parsed;
    parsed.original = std::string(token);

    const auto trimmed = detail::trim_copy(token);

    if (trimmed == "--") {
        parsed.is_end_of_flags = true;
        return parsed;
    }

    if (detail::starts_with(trimmed, "--no-")) {
        parsed.key = trimmed.substr(5);
        parsed.is_flag = true;
        parsed.is_negation = true;
        return parsed;
    }

    if (detail::starts_with(trimmed, "--")) {
        const auto separator = trimmed.find('=');
        parsed.is_flag = true;

        if (separator != std::string::npos) {
            parsed.key = trimmed.substr(2, separator - 2);
            parsed.value = trimmed.substr(separator + 1);
        } else {
            parsed.key = trimmed.substr(2);
        }

        return parsed;
    }

    if (trimmed.size() > 1 && trimmed[0] == k_option_prefix && trimmed[1] != k_option_prefix) {
        const auto body = trimmed.substr(1);
        const auto separator = body.find('=');
        parsed.is_flag = true;
        if (separator != std::string::npos) {
            parsed.key = body.substr(0, separator);
            parsed.value = body.substr(separator + 1);
        } else {
            parsed.key = body;
        }
        parsed.is_grouped = !parsed.value.has_value() && parsed.key.size() > 1;
        return parsed;
    }

    return parsed;
}

}  // namespace clix
