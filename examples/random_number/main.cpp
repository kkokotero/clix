#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <string>
#include <vector>

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

}  // namespace

int main(int argc, char** argv) {
    clix::CLI cli("random-number", "0.3.1");
    cli.description("Generate one or more random integers.");
    cli.enable_completion();

    cli.opt("min", clix::ValueKind::number)
        .description("Smallest number in the range.")
        .label("number")
        .default_value(clix::CliValue(1.0))
        .validate(whole_number());

    cli.opt("max", clix::ValueKind::number)
        .description("Largest number in the range.")
        .label("number")
        .default_value(clix::CliValue(100.0))
        .validate(whole_number());

    cli.opt("count", clix::ValueKind::number)
        .alias("c")
        .description("How many numbers to generate.")
        .label("count")
        .default_value(clix::CliValue(1.0))
        .validate(clix::validators::number_range(1.0, 100.0))
        .validate(whole_number());

    cli.opt("seed", clix::ValueKind::number)
        .description("Optional deterministic seed.")
        .label("number")
        .validate(whole_number());

    cli.opt("unique")
        .description("Do not repeat numbers.");

    cli.opt("format", clix::ValueKind::choice)
        .description("Output format.")
        .label("format")
        .default_value(clix::CliValue(std::string("lines")))
        .choices({"lines", "csv"});

    cli.action([](const clix::Invocation& invocation) {
        const auto minimum = static_cast<int>(invocation.option<double>("min"));
        const auto maximum = static_cast<int>(invocation.option<double>("max"));
        const auto count = static_cast<int>(invocation.option<double>("count"));
        const auto unique = invocation.option<bool>("unique");

        if (maximum < minimum) {
            throw std::runtime_error("The maximum value must be greater than or equal to the minimum value.");
        }

        const auto range_size = maximum - minimum + 1;
        if (unique && count > range_size) {
            throw std::runtime_error("Cannot generate more unique values than the size of the range.");
        }

        const auto seed = invocation.has_option("seed")
                              ? static_cast<std::uint32_t>(invocation.option<double>("seed"))
                              : std::random_device{}();
        std::mt19937 engine(seed);
        std::vector<int> values;
        values.reserve(static_cast<std::size_t>(count));

        if (unique) {
            values.resize(static_cast<std::size_t>(range_size));
            std::iota(values.begin(), values.end(), minimum);
            std::shuffle(values.begin(), values.end(), engine);
            values.resize(static_cast<std::size_t>(count));
        } else {
            std::uniform_int_distribution<int> distribution(minimum, maximum);
            for (int index = 0; index < count; ++index) {
                values.push_back(distribution(engine));
            }
        }

        const auto format = invocation.option<std::string>("format");
        if (format == "csv") {
            for (std::size_t index = 0; index < values.size(); ++index) {
                if (index > 0) {
                    std::cout << ',';
                }
                std::cout << values[index];
            }
            std::cout << '\n';
            return;
        }

        for (const auto value : values) {
            std::cout << value << '\n';
        }
    });

    return cli.run(argc, argv);
}
