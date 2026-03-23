#pragma once

#include <cctype>
#include <string>
#include <string_view>

#include "../detail/strings.hpp"
#include "../exceptions/parsing_error/index.hpp"

namespace clix {

/**
 * Lightweight URL wrapper used by the CLI parser.
 *
 * The implementation intentionally validates only the high-value structural
 * constraints that are useful for command-line inputs:
 * - a scheme must be present
 * - `://` must be present
 * - the authority segment must not be empty
 * - embedded whitespace and null bytes are rejected
 */
class Url {
public:
    explicit Url(std::string input)
        : raw_(std::move(input)) {
        const auto text = detail::trim_copy(raw_);
        if (text.empty()) {
            throw ParsingError(
                "URL must be a non-empty string",
                ParsingErrorOptions{{}, {}, {}, "Provide a valid absolute URL such as https://example.com."});
        }

        if (text.find('\0') != std::string::npos) {
            throw ParsingError(
                "URL contains an embedded null byte",
                ParsingErrorOptions{{}, {text}, {}, "Embedded null bytes are not valid in URLs."});
        }

        for (const auto character : text) {
            if (std::isspace(static_cast<unsigned char>(character))) {
                throw ParsingError(
                    "URL must not contain whitespace",
                    ParsingErrorOptions{{}, {text}, {}, "Remove spaces and encode them if needed."});
            }
        }

        const auto separator = text.find("://");
        if (separator == std::string::npos) {
            throw ParsingError(
                "URL is missing a scheme",
                ParsingErrorOptions{{}, {text}, {"https://example.com"}, "Use an absolute URL with a scheme."});
        }

        const auto scheme_text = text.substr(0, separator);
        if (!is_valid_scheme(scheme_text)) {
            throw ParsingError(
                "URL scheme is invalid",
                ParsingErrorOptions{{}, {scheme_text}, {"https", "http", "ssh"}, "Use a valid URL scheme."});
        }

        const auto authority_begin = separator + 3;
        const auto authority_end = text.find_first_of("/?#", authority_begin);
        const auto authority_text = text.substr(authority_begin, authority_end - authority_begin);
        if (authority_text.empty()) {
            throw ParsingError(
                "URL authority is missing",
                ParsingErrorOptions{{}, {text}, {"https://example.com"}, "Provide a host after the scheme."});
        }

        raw_ = text;
        scheme_ = detail::to_lower_copy(scheme_text);
    }

    [[nodiscard]] const std::string& raw() const noexcept { return raw_; }
    [[nodiscard]] const std::string& scheme() const noexcept { return scheme_; }

    [[nodiscard]] bool is_secure() const noexcept { return scheme_ == "https"; }

    [[nodiscard]] std::string authority() const {
        const auto separator = raw_.find("://");
        const auto authority_begin = separator + 3;
        const auto authority_end = raw_.find_first_of("/?#", authority_begin);
        return raw_.substr(authority_begin, authority_end - authority_begin);
    }

    [[nodiscard]] std::string path() const {
        const auto authority_end = raw_.find_first_of("/?#", raw_.find("://") + 3);
        if (authority_end == std::string::npos || raw_[authority_end] != '/') {
            return "/";
        }

        const auto path_end = raw_.find_first_of("?#", authority_end);
        return raw_.substr(authority_end, path_end - authority_end);
    }

    [[nodiscard]] std::string query() const {
        const auto query_start = raw_.find('?');
        if (query_start == std::string::npos) {
            return {};
        }

        const auto query_end = raw_.find('#', query_start + 1);
        return raw_.substr(query_start + 1, query_end - query_start - 1);
    }

    [[nodiscard]] std::string fragment() const {
        const auto fragment_start = raw_.find('#');
        if (fragment_start == std::string::npos) {
            return {};
        }

        return raw_.substr(fragment_start + 1);
    }

    [[nodiscard]] std::string string() const { return raw_; }
    [[nodiscard]] std::string to_string() const { return raw_; }

    [[nodiscard]] friend bool operator==(const Url& left, const Url& right) {
        return left.raw_ == right.raw_ && left.scheme_ == right.scheme_;
    }

    static Url parse(std::string_view input) { return Url(std::string(input)); }

    [[nodiscard]] static bool is_valid(std::string_view input) {
        try {
            (void)Url(std::string(input));
            return true;
        } catch (...) {
            return false;
        }
    }

private:
    [[nodiscard]] static bool is_valid_scheme(std::string_view value) {
        if (value.empty() || !std::isalpha(static_cast<unsigned char>(value.front()))) {
            return false;
        }

        for (const auto character : value) {
            if (std::isalnum(static_cast<unsigned char>(character)) || character == '+' || character == '-' ||
                character == '.') {
                continue;
            }
            return false;
        }

        return true;
    }

    std::string raw_;
    std::string scheme_;
};

}  // namespace clix
