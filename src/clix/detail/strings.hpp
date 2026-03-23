#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace clix::detail {

inline bool starts_with(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

inline bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.substr(value.size() - suffix.size(), suffix.size()) == suffix;
}

inline bool contains(std::string_view value, std::string_view needle) {
    return value.find(needle) != std::string_view::npos;
}

inline bool contains(std::string_view value, char needle) {
    return value.find(needle) != std::string_view::npos;
}

template <typename Map, typename Key>
inline bool contains_key(const Map& map, const Key& key) {
    return map.find(key) != map.end();
}

inline std::string trim_copy(std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\n\r\f\v");
    if (begin == std::string_view::npos) {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\n\r\f\v");
    return std::string(value.substr(begin, end - begin + 1));
}

inline std::string to_lower_copy(std::string_view value) {
    auto result = std::string(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

inline std::string replace_all_copy(std::string value,
                                    std::string_view needle,
                                    std::string_view replacement) {
    if (needle.empty()) {
        return value;
    }

    std::size_t cursor = 0;
    while ((cursor = value.find(needle, cursor)) != std::string::npos) {
        value.replace(cursor, needle.size(), replacement);
        cursor += replacement.size();
    }

    return value;
}

inline std::string escape_quotes(std::string_view value) {
    auto escaped = std::string(value);
    escaped = replace_all_copy(std::move(escaped), "\\", "\\\\");
    escaped = replace_all_copy(std::move(escaped), "\"", "\\\"");
    return escaped;
}

inline std::string quote(std::string_view value) {
    return "\"" + escape_quotes(value) + "\"";
}

inline std::string number_to_string(double value) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return value < 0.0 ? "-inf" : "inf";
    }

    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    auto text = stream.str();

    const auto exponent = text.find_first_of("eE");
    if (exponent != std::string::npos) {
        return text;
    }

    const auto decimal = text.find('.');
    if (decimal == std::string::npos) {
        return text;
    }

    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }

    return text.empty() ? "0" : text;
}

inline std::string pad_right(std::string_view value, std::size_t width) {
    auto result = std::string(value);
    if (result.size() < width) {
        result.append(width - result.size(), ' ');
    }
    return result;
}

inline std::string join_strings(const std::vector<std::string>& values, std::string_view separator) {
    std::string result;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            result += separator;
        }
        result += values[index];
    }
    return result;
}

inline std::vector<std::string> split(std::string_view value, char delimiter) {
    std::vector<std::string> parts;
    std::size_t cursor = 0;

    while (cursor <= value.size()) {
        const auto next = value.find(delimiter, cursor);
        if (next == std::string_view::npos) {
            parts.emplace_back(value.substr(cursor));
            break;
        }

        parts.emplace_back(value.substr(cursor, next - cursor));
        cursor = next + 1;
    }

    return parts;
}

inline std::vector<std::string> split_lines(std::string_view value) {
    return split(value, '\n');
}

}  // namespace clix::detail
