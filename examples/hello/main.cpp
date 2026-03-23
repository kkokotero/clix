#include <cctype>
#include <iostream>
#include <string>

#include <clix/cli.hpp>
#include <clix/validators.hpp>

int main(int argc, char** argv) {
    clix::CLI cli("hello", "0.2.1");
    cli.description("Simple greeting example built with clix.");
    cli.enable_completion();

    cli.arg("name")
        .description("Name to greet.")
        .label("name")
        .optional()
        .default_value(clix::CliValue(std::string("world")))
        .validate(clix::validators::non_empty_string());

    cli.opt("from", clix::ValueKind::string)
        .description("Signature shown after the greeting.")
        .label("name")
        .default_value(clix::CliValue(std::string("clix")));

    cli.opt("times", clix::ValueKind::number)
        .alias("t")
        .description("How many greetings to print.")
        .label("count")
        .default_value(clix::CliValue(1.0))
        .validate(clix::validators::number_range(1.0, 10.0));

    cli.opt("caps")
        .alias("c")
        .description("Render the greeting in uppercase.");

    cli.action([](const clix::Invocation& invocation) {
        auto message = std::string("Hello, ") + invocation.argument<std::string>("name") +
                       "! From " + invocation.option<std::string>("from") + ".";

        if (invocation.option<bool>("caps")) {
            for (auto& character : message) {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
        }

        const auto times = static_cast<int>(invocation.option<double>("times"));
        for (int index = 0; index < times; ++index) {
            std::cout << message << '\n';
        }
    });

    return cli.run(argc, argv);
}
