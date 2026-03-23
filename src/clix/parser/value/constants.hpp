#pragma once

#include <array>
#include <string_view>

namespace clix {

inline constexpr std::array<std::string_view, 4> k_true_values = {"true", "1", "yes", "on"};
inline constexpr std::array<std::string_view, 4> k_false_values = {"false", "0", "no", "off"};

}  // namespace clix
