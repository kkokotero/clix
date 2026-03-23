#pragma once

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
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
    url,
    time,
    size,
    json,
    boolean_array,
    string_array,
    number_array,
    path_array,
    url_array,
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
        case ValueKind::url:
            return "url";
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
        case ValueKind::url_array:
            return "url[]";
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
 * Small type-erased wrapper used by `parse_as<T>()` so command handlers can
 * retrieve custom typed values without forcing them into CLIX-specific kinds.
 */
struct TypedValue {
    using formatter_type = std::function<std::string(const std::any&)>;

    template <typename T>
    static std::shared_ptr<TypedValue> create(T value, std::string type_name = {}) {
        auto typed = std::make_shared<TypedValue>();
        typed->storage = std::move(value);
        typed->type_name = type_name.empty() ? std::string("custom") : std::move(type_name);
        return typed;
    }

    template <typename T, typename Formatter>
    static std::shared_ptr<TypedValue> create(T value, std::string type_name, Formatter formatter) {
        auto typed = std::make_shared<TypedValue>();
        typed->storage = std::move(value);
        typed->type_name = type_name.empty() ? std::string("custom") : std::move(type_name);
        typed->formatter = [formatter = std::move(formatter)](const std::any& current) {
            return formatter(std::any_cast<const std::decay_t<T>&>(current));
        };
        return typed;
    }

    template <typename T>
    [[nodiscard]] const std::decay_t<T>* get_if() const {
        return std::any_cast<std::decay_t<T>>(&storage);
    }

    [[nodiscard]] std::string format() const {
        if (formatter) {
            return formatter(storage);
        }
        return "<" + type_name + ">";
    }

    std::any storage;
    std::string type_name = "custom";
    formatter_type formatter;
};

using TypedValuePtr = std::shared_ptr<TypedValue>;

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
                              Url,
                              Time,
                              Size,
                              JsonObject,
                              TypedValuePtr,
                              std::vector<bool>,
                              std::vector<std::string>,
                              std::vector<double>,
                              std::vector<Path>,
                              std::vector<Url>,
                              std::vector<Time>,
                              std::vector<Size>>;

template <typename T, typename Variant>
struct variant_contains;

template <typename T, typename... Ts>
struct variant_contains<T, std::variant<Ts...>> : std::disjunction<std::is_same<T, Ts>...> {};

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
        case ValueKind::url:
            return std::holds_alternative<Url>(value);
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
        case ValueKind::url_array:
            return std::holds_alternative<std::vector<Url>>(value);
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
                                 std::is_same_v<current_type, Url> ||
                                 std::is_same_v<current_type, Time> ||
                                 std::is_same_v<current_type, Size>) {
                return current.to_string();
            } else if constexpr (std::is_same_v<current_type, JsonObject>) {
                return JsonValue(current).dump();
            } else if constexpr (std::is_same_v<current_type, TypedValuePtr>) {
                return current == nullptr ? std::string("<custom>") : current->format();
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

template <typename T>
[[nodiscard]] inline const std::decay_t<T>& value_cast(const CliValue& value) {
    using value_type = std::decay_t<T>;

    if constexpr (variant_contains<value_type, CliValue>::value) {
        if (const auto* direct = std::get_if<value_type>(&value); direct != nullptr) {
            return *direct;
        }
    }

    if (const auto* typed = std::get_if<TypedValuePtr>(&value); typed != nullptr && *typed != nullptr) {
        if (const auto* custom = (*typed)->template get_if<value_type>(); custom != nullptr) {
            return *custom;
        }
    }

    throw std::bad_variant_access();
}

[[nodiscard]] inline bool is_typed_value(const CliValue& value) {
    return std::holds_alternative<TypedValuePtr>(value);
}

template <typename T>
[[nodiscard]] inline CliValue make_typed_value(T value, std::string type_name = {}) {
    return CliValue(TypedValue::create(std::move(value), std::move(type_name)));
}

template <typename T, typename Formatter>
[[nodiscard]] inline CliValue make_typed_value(T value, std::string type_name, Formatter formatter) {
    return CliValue(TypedValue::create(std::move(value), std::move(type_name), std::move(formatter)));
}

}  // namespace clix
