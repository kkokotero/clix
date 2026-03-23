#pragma once

#include <cmath>
#include <functional>
#include <optional>
#include <regex>
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

inline ValidatorSpec all_of(std::vector<ValidatorSpec> validators,
                            std::string name = "all_of") {
    return ValidatorSpec{
        std::move(name),
        [validators = std::move(validators)](const CliValue& value,
                                             const ValidatorContext& context) -> std::optional<std::string> {
            for (const auto& validator : validators) {
                if (!validator.validate) {
                    continue;
                }

                if (const auto result = validator.validate(value, context); result.has_value()) {
                    return result;
                }
            }
            return std::nullopt;
        }};
}

inline ValidatorSpec any_of(std::vector<ValidatorSpec> validators,
                            std::string name = "any_of") {
    return ValidatorSpec{
        std::move(name),
        [validators = std::move(validators)](const CliValue& value,
                                             const ValidatorContext& context) -> std::optional<std::string> {
            std::vector<std::string> failures;
            for (const auto& validator : validators) {
                if (!validator.validate) {
                    continue;
                }

                if (const auto result = validator.validate(value, context); !result.has_value()) {
                    return std::nullopt;
                } else {
                    failures.push_back(*result);
                }
            }

            if (failures.empty()) {
                return std::nullopt;
            }

            return "Value must satisfy at least one validator: " + detail::join_strings(failures, " | ");
        }};
}

inline ValidatorSpec none_of(std::vector<ValidatorSpec> validators,
                             std::string name = "none_of") {
    return ValidatorSpec{
        std::move(name),
        [validators = std::move(validators)](const CliValue& value,
                                             const ValidatorContext& context) -> std::optional<std::string> {
            for (const auto& validator : validators) {
                if (!validator.validate) {
                    continue;
                }

                if (const auto result = validator.validate(value, context); !result.has_value()) {
                    return "Value matched a forbidden validator" +
                           (validator.name.empty() ? std::string(".") : ": " + validator.name + ".");
                }
            }
            return std::nullopt;
        }};
}

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

inline ValidatorSpec matches(std::string pattern,
                             std::string name = "matches") {
    return ValidatorSpec{
        std::move(name),
        [regex = std::regex(std::move(pattern))](const CliValue& value,
                                                 const ValidatorContext&) -> std::optional<std::string> {
            if (const auto* string = std::get_if<std::string>(&value); string != nullptr &&
                !std::regex_match(*string, regex)) {
                return "Value does not match the required pattern.";
            }
            return std::nullopt;
        }};
}

inline ValidatorSpec length(std::size_t minimum,
                            std::optional<std::size_t> maximum = std::nullopt,
                            std::string name = "length") {
    return ValidatorSpec{
        std::move(name),
        [minimum, maximum](const CliValue& value, const ValidatorContext&) -> std::optional<std::string> {
            if (const auto* string = std::get_if<std::string>(&value); string != nullptr) {
                if (string->size() < minimum) {
                    return "Value length must be at least " + std::to_string(minimum) + ".";
                }
                if (maximum.has_value() && string->size() > *maximum) {
                    return "Value length must be at most " + std::to_string(*maximum) + ".";
                }
            }
            return std::nullopt;
        }};
}

}  // namespace validators

}  // namespace clix
