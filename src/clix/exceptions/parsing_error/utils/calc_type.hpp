#pragma once

#include <cctype>
#include <regex>
#include <string>
#include <string_view>

#include "../../../detail/strings.hpp"
#include "is_valid_number_string.hpp"

namespace clix {

namespace detail {

inline bool looks_like_size(std::string_view value) {
    static const std::regex pattern(R"(^\d+(?:\.\d+)?(b|kb|mb|gb|tb|pb)$)", std::regex::icase);
    return std::regex_match(value.begin(), value.end(), pattern);
}

inline bool looks_like_time(std::string_view value) {
    static const std::regex duration_pattern(
        R"(^(\d+(?:\.\d+)?)(ms|s|m|h|d|w|mo|y)(\s*\d+(?:\.\d+)?(ms|s|m|h|d|w|mo|y))*$)",
        std::regex::icase);

    static const std::regex date_pattern(
        R"(^(\d{4}[-/]\d{2}[-/]\d{2}|\d{2}-\d{2}-\d{2,4})(\s+\d{2}:\d{2}(:\d{2})?)?$)");

    return std::regex_match(value.begin(), value.end(), duration_pattern) ||
           std::regex_match(value.begin(), value.end(), date_pattern);
}

inline bool looks_like_path(std::string_view value) {
    return value.find('/') != std::string_view::npos || value.find('\\') != std::string_view::npos ||
           detail::starts_with(value, "./") || detail::starts_with(value, "../") ||
           detail::starts_with(value, "~/");
}

}  // namespace detail

/**
 * Best-effort logical type inference used to build friendlier parser errors.
 */
inline std::string calc_type(std::string_view value) {
    const auto trimmed = detail::trim_copy(value);
    if (trimmed.empty()) {
        return "empty";
    }

    if (trimmed == "true" || trimmed == "false") {
        return "boolean";
    }

    if (is_valid_number_string(trimmed)) {
        return "number";
    }

    if (trimmed.front() == '{' && trimmed.back() == '}') {
        return "object";
    }

    if (trimmed.front() == '[' && trimmed.back() == ']') {
        return "array";
    }

    if (trimmed.find(',') != std::string::npos) {
        return "list";
    }

    if (detail::looks_like_size(trimmed)) {
        return "size";
    }

    if (detail::looks_like_time(trimmed)) {
        return "time";
    }

    if (detail::looks_like_path(trimmed)) {
        return "path";
    }

    return "string";
}

}  // namespace clix
