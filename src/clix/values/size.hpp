#pragma once

#include <cctype>
#include <cmath>
#include <regex>
#include <string>
#include <string_view>

#include "../detail/strings.hpp"
#include "../exceptions/parsing_error/index.hpp"

namespace clix {

/**
 * Represents a byte size parsed from a human-friendly string such as `10kb`
 * or `1.5gb`.
 */
class Size {
public:
    explicit Size(std::string input)
        : raw_(std::move(input))
        , value_(parse_to_bytes(raw_)) {}

    [[nodiscard]] const std::string& raw() const noexcept { return raw_; }
    [[nodiscard]] double bytes() const noexcept { return value_; }
    [[nodiscard]] double kilobytes() const noexcept { return value_ / 1024.0; }
    [[nodiscard]] double megabytes() const noexcept { return value_ / (1024.0 * 1024.0); }
    [[nodiscard]] double gigabytes() const noexcept { return value_ / (1024.0 * 1024.0 * 1024.0); }
    [[nodiscard]] double terabytes() const noexcept { return value_ / std::pow(1024.0, 4.0); }
    [[nodiscard]] double petabytes() const noexcept { return value_ / std::pow(1024.0, 5.0); }
    [[nodiscard]] bool is_zero() const noexcept { return value_ == 0.0; }
    [[nodiscard]] bool is_positive() const noexcept { return value_ > 0.0; }

    [[nodiscard]] std::string string() const { return detail::number_to_string(value_) + "B"; }
    [[nodiscard]] std::string to_string() const { return string(); }

    [[nodiscard]] friend bool operator==(const Size& left, const Size& right) {
        return left.raw_ == right.raw_ && left.value_ == right.value_;
    }

    static Size parse(std::string_view input) { return Size(std::string(input)); }

    [[nodiscard]] static bool is_valid(std::string_view input) {
        static const std::regex pattern(R"(^\s*(\d+(?:\.\d+)?)(b|kb|mb|gb|tb|pb)\s*$)",
                                        std::regex::icase);

        return std::regex_match(input.begin(), input.end(), pattern);
    }

private:
    [[nodiscard]] static double parse_to_bytes(const std::string& input) {
        if (trim(input).empty()) {
            throw ParsingError(
                "Size must be a non-empty string",
                ParsingErrorOptions{{}, {}, {}, "Provide a valid size such as 10kb or 5mb."});
        }

        static const std::regex pattern(R"(^\s*(\d+(?:\.\d+)?)(b|kb|mb|gb|tb|pb)\s*$)",
                                        std::regex::icase);

        std::smatch match;
        if (!std::regex_match(input, match, pattern)) {
            throw ParsingError(
                "Invalid size format",
                ParsingErrorOptions{{},
                                    {input},
                                    {"10kb", "5mb", "1gb", "2tb", "500b"},
                                    "Use a number followed by b, kb, mb, gb, tb, or pb."});
        }

        const auto value = std::stod(match[1].str());
        auto unit = match[2].str();
        for (auto& character : unit) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        if (unit == "b") {
            return value;
        }
        if (unit == "kb") {
            return value * 1024.0;
        }
        if (unit == "mb") {
            return value * std::pow(1024.0, 2.0);
        }
        if (unit == "gb") {
            return value * std::pow(1024.0, 3.0);
        }
        if (unit == "tb") {
            return value * std::pow(1024.0, 4.0);
        }
        if (unit == "pb") {
            return value * std::pow(1024.0, 5.0);
        }

        throw ParsingError(
            "Unsupported size unit",
            ParsingErrorOptions{{"b", "kb", "mb", "gb", "tb", "pb"},
                                {unit},
                                {},
                                "Use one of the supported size units."});
    }

    [[nodiscard]] static std::string trim(std::string_view value) {
        const auto begin = value.find_first_not_of(" \t\n\r\f\v");
        if (begin == std::string_view::npos) {
            return {};
        }

        const auto end = value.find_last_not_of(" \t\n\r\f\v");
        return std::string(value.substr(begin, end - begin + 1));
    }

    std::string raw_;
    double value_ = 0.0;
};

}  // namespace clix
