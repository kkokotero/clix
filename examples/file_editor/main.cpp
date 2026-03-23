#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include <clix/cli.hpp>
#include <clix/router.hpp>
#include <clix/validators.hpp>

#include "file_service.hpp"

namespace {

void print_with_line_numbers(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::size_t line_number = 1;

    while (std::getline(stream, line)) {
        std::cout << line_number++ << ": " << line << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    clix::CLI cli("file-editor", "0.2.1");
    cli.description("Read and edit small text files.");
    cli.enable_completion();

    clix::Router router;

    router.command("show", [](clix::Command& command) {
        command.description("Print a text file.");
        command.arg("path", clix::ValueKind::path)
            .description("File to print.")
            .label("path")
            .validate(clix::validators::existing_path());
        command.opt("line-numbers")
            .description("Print line numbers before each line.");

        command.action([](const clix::Invocation& invocation) {
            const auto path = invocation.argument<clix::Path>("path").value();
            const auto content = examples::file_editor::FileService::read_text(path);
            if (invocation.option<bool>("line-numbers")) {
                print_with_line_numbers(content);
                return;
            }
            std::cout << content;
            if (!content.empty() && content.back() != '\n') {
                std::cout << '\n';
            }
        });
    });

    router.command("write", [](clix::Command& command) {
        command.description("Overwrite a file with new content.");
        command.arg("path", clix::ValueKind::path)
            .description("File to write.")
            .label("path");
        command.opt("text", clix::ValueKind::string)
            .alias("t")
            .description("Text written into the file.")
            .label("text")
            .required();

        command.action([](const clix::Invocation& invocation) {
            const auto path = invocation.argument<clix::Path>("path").value();
            examples::file_editor::FileService::write_text(path, invocation.option<std::string>("text"));
            std::cout << "Wrote " << path.string() << '\n';
        });
    });

    router.command("append", [](clix::Command& command) {
        command.description("Append text to an existing file.");
        command.arg("path", clix::ValueKind::path)
            .description("File to append to.")
            .label("path")
            .validate(clix::validators::existing_path());
        command.opt("text", clix::ValueKind::string)
            .alias("t")
            .description("Text appended to the file.")
            .label("text")
            .required();

        command.action([](const clix::Invocation& invocation) {
            const auto path = invocation.argument<clix::Path>("path").value();
            examples::file_editor::FileService::append_text(path, invocation.option<std::string>("text"));
            std::cout << "Appended to " << path.string() << '\n';
        });
    });

    router.command("replace", [](clix::Command& command) {
        command.description("Replace one string with another inside a file.");
        command.arg("path", clix::ValueKind::path)
            .description("File to edit.")
            .label("path")
            .validate(clix::validators::existing_path());
        command.opt("from", clix::ValueKind::string)
            .description("Text to replace.")
            .label("text")
            .required()
            .validate(clix::validators::non_empty_string());
        command.opt("to", clix::ValueKind::string)
            .description("Replacement text.")
            .label("text")
            .required();

        command.action([](const clix::Invocation& invocation) {
            const auto path = invocation.argument<clix::Path>("path").value();
            const auto count = examples::file_editor::FileService::replace_text(path,
                                                                                invocation.option<std::string>("from"),
                                                                                invocation.option<std::string>("to"));
            std::cout << "Updated " << count << " occurrence(s) in " << path.string() << '\n';
        });
    });

    router.mount(cli);
    return cli.run(argc, argv);
}
