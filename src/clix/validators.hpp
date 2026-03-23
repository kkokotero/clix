#pragma once

#include <cmath>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "detail/strings.hpp"
#include "exceptions/parsing_error/index.hpp"
#include "parser/value/types.hpp"

namespace clix {

struct ValidatorContext {
    std::string command_name;
    std::string value_name;
    bool is_option = false;
    ValueKind kind = ValueKind::string;
};

using Validator = std::function<std::optional<std::string>(const CliValue&, const ValidatorContext&)>;

struct ValidatorSpec {
    std::string name;
    Validator validate;
};

inline void apply_validators(const std::vector<ValidatorSpec>& validators,
                             const CliValue& value,
                             const ValidatorContext& context) {
    for (const auto& validator : validators) {
        if (!validator.validate) {
            continue;
        }

        if (const auto result = validator.validate(value, context); result.has_value()) {
            throw ParsingError(
                "Validation failed",
                ParsingErrorOptions{{},
                                    {format_value(value)},
                                    {},
                                    validator.name.empty()
                                        ? *result
                                        : validator.name + ": " + *result});
        }
    }
}

namespace validators {

inline ValidatorSpec non_empty_string(std::string name = "non_empty_string") {
    return ValidatorSpec{
        std::move(name),
        [](const CliValue& value, const ValidatorContext&) -> std::optional<std::string> {
            if (const auto* string = std::get_if<std::string>(&value); string != nullptr) {
                return string->empty() ? std::optional<std::string>("Value must not be empty.") : std::nullopt;
            }
            return std::nullopt;
        }};
}

inline ValidatorSpec number_range(double minimum,
                                  double maximum,
                                  std::string name = "number_range") {
    return ValidatorSpec{
        std::move(name),
        [minimum, maximum](const CliValue& value, const ValidatorContext&) -> std::optional<std::string> {
            if (const auto* number = std::get_if<double>(&value); number != nullptr) {
                if (*number < minimum || *number > maximum) {
                    return "Value must be between " + detail::number_to_string(minimum) + " and " +
                           detail::number_to_string(maximum) + ".";
                }
            }
            return std::nullopt;
        }};
}

inline ValidatorSpec existing_path(std::string name = "existing_path") {
    return ValidatorSpec{
        std::move(name),
        [](const CliValue& value, const ValidatorContext&) -> std::optional<std::string> {
            if (const auto* path = std::get_if<Path>(&value); path != nullptr && !path->exists()) {
                return "Path must exist.";
            }
            return std::nullopt;
        }};
}

inline ValidatorSpec positive_number(std::string name = "positive_number") {
    return ValidatorSpec{
        std::move(name),
        [](const CliValue& value, const ValidatorContext&) -> std::optional<std::string> {
            if (const auto* number = std::get_if<double>(&value); number != nullptr) {
                if (!std::isfinite(*number) || *number <= 0.0) {
                    return "Value must be a positive finite number.";
                }
            }
            return std::nullopt;
        }};
}

}  // namespace validators

}  // namespace clix
