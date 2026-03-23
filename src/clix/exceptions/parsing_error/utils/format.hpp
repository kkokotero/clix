#pragma once

#include <string>
#include <vector>

#include "../../../detail/strings.hpp"
#include "calc_type.hpp"

namespace clix {

inline std::string format_list(const std::vector<std::string>& items) {
    std::string result;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            result += ", ";
        }
        result += detail::quote(items[index]);
    }
    return result;
}

inline std::string format_received(const std::vector<std::string>& items) {
    std::string result;
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) {
            result += ", ";
        }

        result += detail::quote(items[index]) + " -> (" + calc_type(items[index]) + ")";
    }
    return result;
}

inline std::string format_examples(const std::vector<std::string>& examples) {
    std::string result;
    for (std::size_t index = 0; index < examples.size(); ++index) {
        if (index > 0) {
            result += ", ";
        }
        result += examples[index];
    }
    return result;
}

}  // namespace clix
