#pragma once

#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../detail/strings.hpp"
#include "../exceptions/parsing_error/index.hpp"

namespace clix {

/**
 * Represents either a relative duration or a future/past date converted to
 * milliseconds.
 */
class Time {
public:
    explicit Time(std::string input)
        : raw_(std::move(input))
        , value_(parse_to_milliseconds(raw_)) {}

    [[nodiscard]] const std::string& raw() const noexcept { return raw_; }
    [[nodiscard]] double milliseconds() const noexcept { return value_; }
    [[nodiscard]] double seconds() const noexcept { return value_ / 1000.0; }
    [[nodiscard]] double minutes() const noexcept { return value_ / 60000.0; }
    [[nodiscard]] double hours() const noexcept { return value_ / 3600000.0; }
    [[nodiscard]] double days() const noexcept { return value_ / 86400000.0; }
    [[nodiscard]] bool is_zero() const noexcept { return value_ == 0.0; }
    [[nodiscard]] bool is_positive() const noexcept { return value_ > 0.0; }

    [[nodiscard]] std::string string() const { return detail::number_to_string(value_) + "ms"; }
    [[nodiscard]] std::string to_string() const { return string(); }

    [[nodiscard]] friend bool operator==(const Time& left, const Time& right) {
        return left.raw_ == right.raw_ && left.value_ == right.value_;
    }

    static Time parse(std::string_view input) { return Time(std::string(input)); }

    [[nodiscard]] static bool is_valid(std::string_view input) {
        const auto text = trim(input);
        if (text.empty()) {
            return false;
        }

        return parse_date(text).has_value() || parse_duration(text).has_value();
    }

private:
    [[nodiscard]] static double parse_to_milliseconds(const std::string& input) {
        const auto text = trim(input);
        if (text.empty()) {
            throw ParsingError(
                "Time must be a non-empty string",
                ParsingErrorOptions{{}, {}, {}, "Provide a duration like 5s or a date like 2026-12-28 14:00."});
        }

        if (const auto date = parse_date(text); date.has_value()) {
            return *date;
        }

        if (const auto duration = parse_duration(text); duration.has_value()) {
            return *duration;
        }

        throw ParsingError(
            "Invalid time format",
            ParsingErrorOptions{{},
                                {input},
                                {"5s", "1h30m", "2026-12-28 14:00"},
                                "Use a duration such as 5s, 1h30m, 2d, or a date/time string."});
    }

    [[nodiscard]] static std::optional<double> parse_date(std::string text) {
        text = normalize_date_string(std::move(text));

        static const std::vector<std::string> formats = {
            "%Y-%m-%d %H:%M:%S",
            "%Y-%m-%d %H:%M",
            "%Y-%m-%d",
        };

        for (const auto& format : formats) {
            std::tm tm = {};
            tm.tm_isdst = -1;

            std::istringstream stream(text);
            stream >> std::get_time(&tm, format.c_str());

            if (stream.fail() || stream.peek() != std::char_traits<char>::eof()) {
                continue;
            }

            const auto timestamp = std::mktime(&tm);
            if (timestamp == -1) {
                continue;
            }

            const auto now = std::chrono::system_clock::now();
            const auto target = std::chrono::system_clock::from_time_t(timestamp);
            const auto delta = target - now;
            return static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(delta).count());
        }

        return std::nullopt;
    }

    [[nodiscard]] static std::optional<double> parse_duration(const std::string& input) {
        static const std::regex pattern(R"((\d+(?:\.\d+)?)(ms|s|m|h|d|w|mo|y))",
                                        std::regex::icase);

        std::sregex_iterator iterator(input.begin(), input.end(), pattern);
        std::sregex_iterator end;

        double total = 0.0;
        std::size_t cursor = 0;
        bool found = false;

        for (; iterator != end; ++iterator) {
            found = true;

            const auto& match = *iterator;
            const auto match_position = static_cast<std::size_t>(match.position());
            if (!trim(std::string_view(input).substr(cursor, match_position - cursor)).empty()) {
                return std::nullopt;
            }

            const auto value = std::stod(match[1].str());
            auto unit = match[2].str();
            for (auto& character : unit) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }

            total += value * unit_to_milliseconds(unit);
            cursor = match_position + match.length();
        }

        if (!found || !trim(std::string_view(input).substr(cursor)).empty()) {
            return std::nullopt;
        }

        return total;
    }

    [[nodiscard]] static double unit_to_milliseconds(std::string_view unit) {
        if (unit == "ms") {
            return 1.0;
        }
        if (unit == "s") {
            return 1000.0;
        }
        if (unit == "m") {
            return 60.0 * 1000.0;
        }
        if (unit == "h") {
            return 60.0 * 60.0 * 1000.0;
        }
        if (unit == "d") {
            return 24.0 * 60.0 * 60.0 * 1000.0;
        }
        if (unit == "w") {
            return 7.0 * 24.0 * 60.0 * 60.0 * 1000.0;
        }
        if (unit == "mo") {
            return 30.44 * 24.0 * 60.0 * 60.0 * 1000.0;
        }
        if (unit == "y") {
            return 365.25 * 24.0 * 60.0 * 60.0 * 1000.0;
        }

        throw ParsingError(
            "Unsupported time unit",
            ParsingErrorOptions{{"ms", "s", "m", "h", "d", "w", "mo", "y"},
                                {std::string(unit)},
                                {},
                                "Use one of the supported time units."});
    }

    [[nodiscard]] static std::string normalize_date_string(std::string text) {
        for (auto& character : text) {
            if (character == '/') {
                character = '-';
            }
        }

        static const std::regex day_first_pattern(R"(^(\d{2})-(\d{2})-(\d{2,4})(.*)$)");
        std::smatch match;
        if (std::regex_match(text, match, day_first_pattern)) {
            auto year = match[3].str();
            if (year.size() == 2) {
                year = "20" + year;
            }

            text = year + "-" + match[2].str() + "-" + match[1].str() + match[4].str();
        }

        return text;
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
