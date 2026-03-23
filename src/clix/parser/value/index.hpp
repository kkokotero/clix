#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "../../exceptions/index.hpp"
#include "../../values/index.hpp"
#include "types.hpp"
#include "utils/parse_boolean.hpp"
#include "utils/parse_choice.hpp"
#include "utils/parse_json.hpp"
#include "utils/parse_list.hpp"
#include "utils/parse_number.hpp"

namespace clix {

inline CliValue parse_value(ValueKind kind,
                            std::string_view value,
                            const std::vector<std::string>& choices = {}) {
    switch (kind) {
        case ValueKind::boolean:
            return parse_boolean(value);
        case ValueKind::string:
            return std::string(value);
        case ValueKind::number:
            return parse_number(value);
        case ValueKind::choice:
            return parse_choice(value, choices);
        case ValueKind::path:
            return Path::parse(value);
        case ValueKind::url:
            return Url::parse(value);
        case ValueKind::time:
            return Time::parse(value);
        case ValueKind::size:
            return Size::parse(value);
        case ValueKind::json:
            return parse_json(value);
        case ValueKind::boolean_array: {
            std::vector<bool> values;
            for (const auto& item : parse_list(value)) {
                values.push_back(parse_boolean(item));
            }
            return values;
        }
        case ValueKind::string_array:
            return parse_list(value);
        case ValueKind::number_array: {
            std::vector<double> values;
            for (const auto& item : parse_list(value)) {
                values.push_back(parse_number(item));
            }
            return values;
        }
        case ValueKind::path_array: {
            std::vector<Path> values;
            for (const auto& item : parse_list(value)) {
                values.push_back(Path::parse(item));
            }
            return values;
        }
        case ValueKind::url_array: {
            std::vector<Url> values;
            for (const auto& item : parse_list(value)) {
                values.push_back(Url::parse(item));
            }
            return values;
        }
        case ValueKind::time_array: {
            std::vector<Time> values;
            for (const auto& item : parse_list(value)) {
                values.push_back(Time::parse(item));
            }
            return values;
        }
        case ValueKind::size_array: {
            std::vector<Size> values;
            for (const auto& item : parse_list(value)) {
                values.push_back(Size::parse(item));
            }
            return values;
        }
    }

    throw ParsingError(
        "Unsupported value kind",
        ParsingErrorOptions{{},
                            {to_string(kind)},
                            {},
                            "Use one of the supported value kinds exposed by clix::ValueKind."});
}

inline CliValue coerce_argument(ValueKind kind,
                                std::string_view raw,
                                const std::vector<std::string>& choices = {}) {
    return parse_value(kind, raw, choices);
}

inline CliValue coerce_option(ValueKind kind,
                              const std::string& raw,
                              const std::vector<std::string>& choices = {}) {
    return parse_value(kind, raw, choices);
}

}  // namespace clix
