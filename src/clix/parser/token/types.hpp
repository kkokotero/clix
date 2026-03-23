#pragma once

#include <optional>
#include <string>

namespace clix {

/**
 * Normalized token representation used by the command parser.
 */
struct ParsedToken {
    std::string key;
    std::optional<std::string> value;
    bool is_flag = false;
    bool is_negation = false;
    bool is_grouped = false;
    bool is_end_of_flags = false;
    std::string original;
};

}  // namespace clix
