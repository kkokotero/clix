#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

#include <clix/cli.hpp>
#include <clix/validators.hpp>

namespace {

clix::ValidatorSpec whole_number(std::string name = "whole_number") {
    return clix::ValidatorSpec{
        std::move(name),
        [](const clix::CliValue& value, const clix::ValidatorContext&) -> std::optional<std::string> {
            const auto* number = std::get_if<double>(&value);
            if (number != nullptr && std::floor(*number) != *number) {
                return std::string("Value must be a whole number.");
            }
            return std::nullopt;
        }};
}

clix::ValidatorSpec non_zero(std::string name = "non_zero") {
    return clix::ValidatorSpec{
        std::move(name),
        [](const clix::CliValue& value, const clix::ValidatorContext&) -> std::optional<std::string> {
            const auto* number = std::get_if<double>(&value);
            if (number != nullptr && *number == 0.0) {
                return std::string("Value must not be zero.");
            }
            return std::nullopt;
        }};
}

void add_binary_command(clix::CLI& cli,
                        const std::string& name,
                        const std::string& description,
                        std::function<double(double, double)> operation,
                        const std::optional<clix::ValidatorSpec>& right_validator = std::nullopt) {
    auto& command = cli.command(name).description(description);
    command.arg("left", clix::ValueKind::number)
        .description("Left operand.")
        .label("number");
    auto right = command.arg("right", clix::ValueKind::number);
    right
        .description("Right operand.")
        .label("number");

    if (right_validator.has_value()) {
        right.validate(*right_validator);
    }

    command.action([operation = std::move(operation)](const clix::Invocation& invocation) {
        const auto left = invocation.argument<double>("left");
        const auto right = invocation.argument<double>("right");
        const auto result = operation(left, right);
        const auto precision = static_cast<int>(invocation.option<double>("precision"));

        if (invocation.option<bool>("scientific")) {
            std::cout << std::scientific;
        } else {
            std::cout << std::fixed;
        }

        std::cout << std::setprecision(precision) << result << '\n';
    });
}

}  // namespace

int main(int argc, char** argv) {
    clix::CLI cli("calculator", "0.3.1");
    cli.description("Small calculator with a few arithmetic subcommands.");
    cli.enable_completion();

    cli.opt("precision", clix::ValueKind::number)
        .alias("p")
        .description("Digits after the decimal separator.")
        .label("digits")
        .default_value(clix::CliValue(2.0))
        .validate(clix::validators::number_range(0.0, 8.0))
        .validate(whole_number());

    cli.opt("scientific")
        .description("Print the result using scientific notation.");

    add_binary_command(cli, "add", "Add two numbers.", [](double left, double right) { return left + right; });
    add_binary_command(cli, "sub", "Subtract two numbers.", [](double left, double right) { return left - right; });
    add_binary_command(cli, "mul", "Multiply two numbers.", [](double left, double right) { return left * right; });
    add_binary_command(cli,
                       "div",
                       "Divide two numbers.",
                       [](double left, double right) { return left / right; },
                       non_zero());

    return cli.run(argc, argv);
}
