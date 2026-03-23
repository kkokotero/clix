#pragma once

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "../../detail/strings.hpp"
#include "../../values/index.hpp"

namespace clix {

/**
 * Supported logical value kinds for arguments and options.
 */
enum class ValueKind {
    boolean,
    string,
    number,
    choice,
    path,
    time,
    size,
    json,
    boolean_array,
    string_array,
    number_array,
    path_array,
    time_array,
    size_array,
};

[[nodiscard]] inline std::string to_string(ValueKind kind) {
    switch (kind) {
        case ValueKind::boolean:
            return "boolean";
        case ValueKind::string:
            return "string";
        case ValueKind::number:
            return "number";
        case ValueKind::choice:
            return "choice";
        case ValueKind::path:
            return "path";
        case ValueKind::time:
            return "time";
        case ValueKind::size:
            return "size";
        case ValueKind::json:
            return "json";
        case ValueKind::boolean_array:
            return "boolean[]";
        case ValueKind::string_array:
            return "string[]";
        case ValueKind::number_array:
            return "number[]";
        case ValueKind::path_array:
            return "path[]";
        case ValueKind::time_array:
            return "time[]";
        case ValueKind::size_array:
            return "size[]";
    }

    return "unknown";
}

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

/**
 * Minimal JSON runtime value used by the `json` coercer without bringing an
 * additional dependency into the header-only distribution.
 */
struct JsonValue {
    using array_ptr = std::shared_ptr<JsonArray>;
    using object_ptr = std::shared_ptr<JsonObject>;
    using storage_type = std::variant<std::nullptr_t, bool, double, std::string, array_ptr, object_ptr>;

    JsonValue() = default;
    JsonValue(std::nullptr_t)
        : storage(nullptr) {}
    JsonValue(bool value)
        : storage(value) {}
    JsonValue(double value)
        : storage(value) {}
    JsonValue(std::string value)
        : storage(std::move(value)) {}
    JsonValue(const char* value)
        : storage(std::string(value)) {}
    JsonValue(JsonArray value)
        : storage(std::make_shared<JsonArray>(std::move(value))) {}
    JsonValue(JsonObject value)
        : storage(std::make_shared<JsonObject>(std::move(value))) {}

    [[nodiscard]] bool is_object() const noexcept {
        return std::holds_alternative<object_ptr>(storage);
    }

    [[nodiscard]] bool is_array() const noexcept {
        return std::holds_alternative<array_ptr>(storage);
    }

    [[nodiscard]] const JsonObject& as_object() const {
        return *std::get<object_ptr>(storage);
    }

    [[nodiscard]] const JsonArray& as_array() const {
        return *std::get<array_ptr>(storage);
    }

    [[nodiscard]] std::string dump() const {
        if (std::holds_alternative<std::nullptr_t>(storage)) {
            return "null";
        }
        if (const auto* boolean = std::get_if<bool>(&storage)) {
            return *boolean ? "true" : "false";
        }
        if (const auto* number = std::get_if<double>(&storage)) {
            return detail::number_to_string(*number);
        }
        if (const auto* string = std::get_if<std::string>(&storage)) {
            return detail::quote(*string);
        }
        if (const auto* array = std::get_if<array_ptr>(&storage)) {
            std::string result = "[";
            for (std::size_t index = 0; index < (*array)->size(); ++index) {
                if (index > 0) {
                    result += ", ";
                }
                result += (*array)->at(index).dump();
            }
            result += "]";
            return result;
        }

        const auto& object = *std::get<object_ptr>(storage);
        std::string result = "{";
        std::size_t index = 0;
        for (const auto& [key, value] : object) {
            if (index++ > 0) {
                result += ", ";
            }
            result += detail::quote(key) + ": " + value.dump();
        }
        result += "}";
        return result;
    }

    storage_type storage = nullptr;
};

using CliValue = std::variant<bool,
                              std::string,
                              double,
                              Path,
                              Time,
                              Size,
                              JsonObject,
                              std::vector<bool>,
                              std::vector<std::string>,
                              std::vector<double>,
                              std::vector<Path>,
                              std::vector<Time>,
                              std::vector<Size>>;

[[nodiscard]] inline bool matches_value_kind(const CliValue& value, ValueKind kind) {
    switch (kind) {
        case ValueKind::boolean:
            return std::holds_alternative<bool>(value);
        case ValueKind::string:
        case ValueKind::choice:
            return std::holds_alternative<std::string>(value);
        case ValueKind::number:
            return std::holds_alternative<double>(value);
        case ValueKind::path:
            return std::holds_alternative<Path>(value);
        case ValueKind::time:
            return std::holds_alternative<Time>(value);
        case ValueKind::size:
            return std::holds_alternative<Size>(value);
        case ValueKind::json:
            return std::holds_alternative<JsonObject>(value);
        case ValueKind::boolean_array:
            return std::holds_alternative<std::vector<bool>>(value);
        case ValueKind::string_array:
            return std::holds_alternative<std::vector<std::string>>(value);
        case ValueKind::number_array:
            return std::holds_alternative<std::vector<double>>(value);
        case ValueKind::path_array:
            return std::holds_alternative<std::vector<Path>>(value);
        case ValueKind::time_array:
            return std::holds_alternative<std::vector<Time>>(value);
        case ValueKind::size_array:
            return std::holds_alternative<std::vector<Size>>(value);
    }

    return false;
}

[[nodiscard]] inline std::string format_value(const CliValue& value) {
    return std::visit(
        [](const auto& current) -> std::string {
            using current_type = std::decay_t<decltype(current)>;

            if constexpr (std::is_same_v<current_type, bool>) {
                return current ? "true" : "false";
            } else if constexpr (std::is_same_v<current_type, std::string>) {
                return current;
            } else if constexpr (std::is_same_v<current_type, double>) {
                return detail::number_to_string(current);
            } else if constexpr (std::is_same_v<current_type, Path> ||
                                 std::is_same_v<current_type, Time> ||
                                 std::is_same_v<current_type, Size>) {
                return current.to_string();
            } else if constexpr (std::is_same_v<current_type, JsonObject>) {
                return JsonValue(current).dump();
            } else {
                std::string result = "[";
                for (std::size_t index = 0; index < current.size(); ++index) {
                    if (index > 0) {
                        result += ", ";
                    }

                    if constexpr (std::is_same_v<typename current_type::value_type, bool>) {
                        result += current[index] ? "true" : "false";
                    } else if constexpr (std::is_same_v<typename current_type::value_type, std::string>) {
                        result += current[index];
                    } else if constexpr (std::is_same_v<typename current_type::value_type, double>) {
                        result += detail::number_to_string(current[index]);
                    } else {
                        result += current[index].to_string();
                    }
                }
                result += "]";
                return result;
            }
        },
        value);
}

}  // namespace clix
