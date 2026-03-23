#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace clix {

namespace detail {

inline double similarity_score(std::string_view left, std::string_view right) {
    if (left.empty() || right.empty()) {
        return 0.0;
    }

    std::size_t matches = 0;
    const auto limit = std::min(left.size(), right.size());

    for (std::size_t index = 0; index < limit; ++index) {
        const auto a = static_cast<char>(std::tolower(static_cast<unsigned char>(left[index])));
        const auto b = static_cast<char>(std::tolower(static_cast<unsigned char>(right[index])));
        if (a == b) {
            ++matches;
        }
    }

    return static_cast<double>(matches) / static_cast<double>(std::max(left.size(), right.size()));
}

}  // namespace detail

/**
 * Resolves a human-friendly hint from the expected and received values.
 */
inline std::string resolve_hint(const std::string& explicit_hint,
                                bool auto_hint,
                                const std::vector<std::string>& expected,
                                const std::vector<std::string>& received) {
    if (!explicit_hint.empty()) {
        return explicit_hint;
    }

    if (!auto_hint || expected.empty() || received.empty()) {
        return {};
    }

    const auto& actual = received.front();
    std::string best_value;
    double best_score = 0.0;

    for (const auto& candidate : expected) {
        const auto score = detail::similarity_score(candidate, actual);
        if (score > best_score) {
            best_score = score;
            best_value = candidate;
        }
    }

    if (best_score >= 0.4) {
        return "Did you mean \"" + best_value + "\"?";
    }

    return {};
}

}  // namespace clix
