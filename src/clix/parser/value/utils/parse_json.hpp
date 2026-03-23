#pragma once

#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <string_view>

#include "../../../exceptions/parsing_error/index.hpp"
#include "../types.hpp"

namespace clix {

namespace detail {

class JsonParser {
public:
    explicit JsonParser(std::string_view input)
        : input_(input) {}

    [[nodiscard]] JsonValue parse() {
        skip_whitespace();
        auto value = parse_value();
        skip_whitespace();

        if (!is_end()) {
            throw ParsingError(
                "Unexpected trailing content in JSON value",
                ParsingErrorOptions{{},
                                    {std::string(input_.substr(position_))},
                                    {},
                                    "Remove extra characters after the JSON object."});
        }

        return value;
    }

private:
    [[nodiscard]] JsonValue parse_value() {
        skip_whitespace();
        if (is_end()) {
            throw ParsingError("Unexpected end of JSON input");
        }

        const auto current = peek();
        if (current == '{') {
            return JsonValue(parse_object());
        }
        if (current == '[') {
            return JsonValue(parse_array());
        }
        if (current == '"') {
            return JsonValue(parse_string());
        }
        if (current == 't') {
            consume_literal("true");
            return JsonValue(true);
        }
        if (current == 'f') {
            consume_literal("false");
            return JsonValue(false);
        }
        if (current == 'n') {
            consume_literal("null");
            return JsonValue(nullptr);
        }
        if (current == '-' || std::isdigit(static_cast<unsigned char>(current)) != 0) {
            return JsonValue(parse_number_literal());
        }

        throw ParsingError(
            "Invalid JSON token",
            ParsingErrorOptions{{},
                                {std::string(1, current)},
                                {},
                                "Expected an object, array, string, number, boolean, or null."});
    }

    [[nodiscard]] JsonObject parse_object() {
        JsonObject object;
        expect('{');
        skip_whitespace();

        if (consume_if('}')) {
            return object;
        }

        while (true) {
            skip_whitespace();
            const auto key = parse_string();

            if (key == "__proto__" || key == "prototype" || key == "constructor") {
                throw ParsingError(
                    "Invalid JSON key",
                    ParsingErrorOptions{{},
                                        {key},
                                        {},
                                        "Keys such as __proto__, prototype, and constructor are not allowed."});
            }

            skip_whitespace();
            expect(':');
            skip_whitespace();
            object.emplace(key, parse_value());
            skip_whitespace();

            if (consume_if('}')) {
                return object;
            }

            expect(',');
        }
    }

    [[nodiscard]] JsonArray parse_array() {
        JsonArray array;
        expect('[');
        skip_whitespace();

        if (consume_if(']')) {
            return array;
        }

        while (true) {
            array.push_back(parse_value());
            skip_whitespace();

            if (consume_if(']')) {
                return array;
            }

            expect(',');
        }
    }

    [[nodiscard]] std::string parse_string() {
        expect('"');
        std::string result;

        while (!is_end()) {
            const auto current = advance();
            if (current == '"') {
                return result;
            }

            if (current == '\\') {
                if (is_end()) {
                    break;
                }

                const auto escaped = advance();
                switch (escaped) {
                    case '"':
                    case '\\':
                    case '/':
                        result.push_back(escaped);
                        break;
                    case 'b':
                        result.push_back('\b');
                        break;
                    case 'f':
                        result.push_back('\f');
                        break;
                    case 'n':
                        result.push_back('\n');
                        break;
                    case 'r':
                        result.push_back('\r');
                        break;
                    case 't':
                        result.push_back('\t');
                        break;
                    default:
                        throw ParsingError(
                            "Unsupported JSON escape sequence",
                            ParsingErrorOptions{{},
                                                {std::string("\\") + escaped},
                                                {},
                                                "Use standard JSON escape sequences."});
                }
                continue;
            }

            result.push_back(current);
        }

        throw ParsingError("Unterminated JSON string literal");
    }

    [[nodiscard]] double parse_number_literal() {
        const auto start = position_;

        if (peek() == '-') {
            ++position_;
        }

        consume_digits();

        if (!is_end() && peek() == '.') {
            ++position_;
            consume_digits();
        }

        if (!is_end() && (peek() == 'e' || peek() == 'E')) {
            ++position_;
            if (!is_end() && (peek() == '+' || peek() == '-')) {
                ++position_;
            }
            consume_digits();
        }

        return std::stod(std::string(input_.substr(start, position_ - start)));
    }

    void consume_literal(std::string_view literal) {
        if (input_.substr(position_, literal.size()) != literal) {
            throw ParsingError(
                "Invalid JSON literal",
                ParsingErrorOptions{{}, {std::string(input_.substr(position_, literal.size()))}});
        }

        position_ += literal.size();
    }

    void consume_digits() {
        const auto start = position_;
        while (!is_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            ++position_;
        }

        if (start == position_) {
            throw ParsingError("Invalid JSON number");
        }
    }

    void skip_whitespace() {
        while (!is_end() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
            ++position_;
        }
    }

    void expect(char expected) {
        if (is_end() || advance() != expected) {
            throw ParsingError(
                "Unexpected JSON token",
                ParsingErrorOptions{{}, {std::string(1, expected)}});
        }
    }

    [[nodiscard]] bool consume_if(char expected) {
        if (!is_end() && peek() == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    [[nodiscard]] char peek() const { return input_[position_]; }

    [[nodiscard]] char advance() { return input_[position_++]; }

    [[nodiscard]] bool is_end() const noexcept { return position_ >= input_.size(); }

    std::string_view input_;
    std::size_t position_ = 0;
};

inline std::optional<std::string> normalize_js_object_literal(std::string_view input) {
    static const std::regex forbidden_tokens(
        R"(\b(function|class|new|return|process|require|import|export|eval|this|global|window)\b)");

    std::string text(input);
    if (std::regex_search(text, forbidden_tokens)) {
        throw ParsingError(
            "Unsafe JavaScript object literal",
            ParsingErrorOptions{{},
                                {std::string(input)},
                                {},
                                "Functions, classes, and runtime expressions are not allowed."});
    }

    const auto begin = text.find_first_not_of(" \t\n\r\f\v");
    if (begin == std::string::npos) {
        return std::nullopt;
    }

    const auto end = text.find_last_not_of(" \t\n\r\f\v");
    text = text.substr(begin, end - begin + 1);

    if (text.front() != '{' && text.front() != '(') {
        return std::nullopt;
    }

    if (text.front() == '(' && text.back() == ')') {
        text = text.substr(1, text.size() - 2);
    }

    text = std::regex_replace(text, std::regex("'"), "\"");
    text = std::regex_replace(text, std::regex(R"(([{,]\s*)([A-Za-z_$][\w$]*)(\s*:))"), "$1\"$2\"$3");
    text = std::regex_replace(text, std::regex(R"(,(\s*[}\]]))"), "$1");

    return text;
}

}  // namespace detail

inline JsonObject parse_json(std::string_view input) {
    try {
        auto parsed = detail::JsonParser(input).parse();
        if (!parsed.is_object()) {
            throw ParsingError(
                "JSON root must be an object",
                ParsingErrorOptions{{"{\"key\": \"value\"}"},
                                    {std::string(input)}});
        }
        return parsed.as_object();
    } catch (const ParsingError&) {
        if (const auto normalized = detail::normalize_js_object_literal(input); normalized.has_value()) {
            auto parsed = detail::JsonParser(*normalized).parse();
            if (!parsed.is_object()) {
                throw ParsingError(
                    "JSON root must be an object",
                    ParsingErrorOptions{{"{\"key\": \"value\"}", "{ key: 'value' }"},
                                        {std::string(input)}});
            }
            return parsed.as_object();
        }
        throw;
    }
}

}  // namespace clix
