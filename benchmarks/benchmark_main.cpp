#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <clix/cli.hpp>
#include <clix/router.hpp>
#include <clix/validators.hpp>

namespace {

class NullBuffer : public std::streambuf {
protected:
    int overflow(int character) override {
        return character;
    }
};

class NullStream : public std::ostream {
public:
    NullStream()
        : std::ostream(&buffer_) {}

private:
    NullBuffer buffer_;
};

volatile std::uint64_t g_sink = 0;

[[nodiscard]] std::unique_ptr<clix::CLI> make_direct_cli() {
    auto cli = std::make_unique<clix::CLI>("workspace", "0.2.0");
    cli->enable_completion();
    cli->enable_config_files();
    cli->opt("verbose").alias("V").description("Enable verbose output.");

    auto& project = cli->command("project").description("Project lifecycle commands.");
    auto& create = project.command("create").description("Create a project from a starter template.");

    create.arg("name")
        .description("Project name.")
        .label("name")
        .validate(clix::validators::non_empty_string());
    create.arg("template", clix::ValueKind::choice)
        .description("Starter template.")
        .label("template")
        .optional()
        .default_value(clix::CliValue(std::string("app")))
        .choices({"app", "lib", "tool"});
    create.opt("language", clix::ValueKind::choice)
        .group("Project")
        .alias("l")
        .description("Target implementation language.")
        .default_value(clix::CliValue(std::string("cpp")))
        .choices({"cpp", "ts"});
    create.opt("jobs", clix::ValueKind::number)
        .group("Project")
        .alias("j")
        .description("Parallel jobs.")
        .default_value(clix::CliValue(1.0))
        .validate(clix::validators::number_range(1.0, 32.0));
    create.opt("region", clix::ValueKind::string)
        .group("Project")
        .description("Target region.")
        .complete([](std::string_view) {
            return std::vector<clix::CompletionSuggestion>{{"us-east-1", "AWS US East"},
                                                           {"eu-west-1", "AWS EU West"},
                                                           {"ap-southeast-1", "AWS AP Southeast"}};
        });
    create.opt("tag", clix::ValueKind::string_array)
        .group("Output")
        .description("Project tags.")
        .complete({{"core", "tag"}, {"cli", "tag"}, {"web", "tag"}});
    create.action([](const clix::Invocation& invocation) {
        g_sink += static_cast<std::uint64_t>(invocation.argument<std::string>("name").size());
        g_sink += static_cast<std::uint64_t>(invocation.option<std::string>("language").size());
    });

    return cli;
}

[[nodiscard]] clix::Router make_router_fixture() {
    clix::Router project_router;
    project_router.root([](clix::Command& project) {
        project.description("Project lifecycle commands.");
    });
    project_router.command("create", [](clix::Command& create) {
        create.description("Create a project from a starter template.");
        create.arg("name")
            .description("Project name.")
            .label("name")
            .validate(clix::validators::non_empty_string());
        create.arg("template", clix::ValueKind::choice)
            .description("Starter template.")
            .label("template")
            .optional()
            .default_value(clix::CliValue(std::string("app")))
            .choices({"app", "lib", "tool"});
        create.opt("language", clix::ValueKind::choice)
            .group("Project")
            .alias("l")
            .description("Target implementation language.")
            .default_value(clix::CliValue(std::string("cpp")))
            .choices({"cpp", "ts"});
        create.opt("jobs", clix::ValueKind::number)
            .group("Project")
            .alias("j")
            .description("Parallel jobs.")
            .default_value(clix::CliValue(1.0))
            .validate(clix::validators::number_range(1.0, 32.0));
        create.opt("region", clix::ValueKind::string)
            .group("Project")
            .description("Target region.")
            .complete([](std::string_view) {
                return std::vector<clix::CompletionSuggestion>{{"us-east-1", "AWS US East"},
                                                               {"eu-west-1", "AWS EU West"},
                                                               {"ap-southeast-1", "AWS AP Southeast"}};
            });
        create.opt("tag", clix::ValueKind::string_array)
            .group("Output")
            .description("Project tags.")
            .complete({{"core", "tag"}, {"cli", "tag"}, {"web", "tag"}});
        create.action([](const clix::Invocation& invocation) {
            g_sink += static_cast<std::uint64_t>(invocation.argument<std::string>("name").size());
            g_sink += static_cast<std::uint64_t>(invocation.option<std::string>("language").size());
        });
    });

    clix::Router app_router;
    app_router.use("project", project_router);
    return app_router;
}

[[nodiscard]] std::unique_ptr<clix::CLI> make_router_cli() {
    auto cli = std::make_unique<clix::CLI>("workspace", "0.2.0");
    cli->enable_completion();
    cli->enable_config_files();
    cli->opt("verbose").alias("V").description("Enable verbose output.");

    auto router = make_router_fixture();
    router.mount(*cli);
    return cli;
}

template <typename Function>
void run_benchmark(std::string_view name, std::size_t iterations, Function&& function) {
    for (std::size_t warmup = 0; warmup < 10; ++warmup) {
        function();
    }

    const auto start = std::chrono::steady_clock::now();
    for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
        function();
    }
    const auto end = std::chrono::steady_clock::now();

    const auto total_ns =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    const auto ns_per_iteration = total_ns / static_cast<double>(iterations);
    const auto ops_per_second = 1'000'000'000.0 / ns_per_iteration;

    std::cout << std::left << std::setw(28) << name
              << std::right << std::setw(14) << static_cast<std::uint64_t>(ns_per_iteration)
              << std::setw(18) << static_cast<std::uint64_t>(ops_per_second)
              << '\n';
}

}  // namespace

int main() {
    NullStream null_out;
    NullStream null_err;

    auto direct_cli = make_direct_cli();
    auto router_cli = make_router_cli();

    std::cout << std::left << std::setw(28) << "benchmark"
              << std::right << std::setw(14) << "ns/op"
              << std::setw(18) << "ops/s"
              << '\n';
    std::cout << std::string(60, '-') << '\n';

    run_benchmark("schema_build_direct", 2'000, [] {
        auto cli = make_direct_cli();
        g_sink += static_cast<std::uint64_t>(cli->name().size());
    });

    run_benchmark("schema_build_router", 2'000, [] {
        auto cli = make_router_cli();
        g_sink += static_cast<std::uint64_t>(cli->name().size());
    });

    run_benchmark("parse_simple_command", 20'000, [&] {
        g_sink += static_cast<std::uint64_t>(direct_cli->run({"project", "create", "widget"}, null_out, null_err));
    });

    run_benchmark("parse_nested_options", 20'000, [&] {
        g_sink += static_cast<std::uint64_t>(
            direct_cli->run({"--verbose",
                             "project",
                             "create",
                             "widget",
                             "--language",
                             "ts",
                             "--jobs",
                             "4",
                             "--region",
                             "us-east-1",
                             "--tag",
                             "core, cli"},
                            null_out,
                            null_err));
    });

    run_benchmark("router_parse_nested", 20'000, [&] {
        g_sink += static_cast<std::uint64_t>(
            router_cli->run({"project", "create", "widget", "--language", "cpp"}, null_out, null_err));
    });

    run_benchmark("completion_choices", 25'000, [&] {
        g_sink += static_cast<std::uint64_t>(direct_cli->complete({"project", "create", "--language", ""}).size());
    });

    run_benchmark("help_generation", 10'000, [&] {
        g_sink += static_cast<std::uint64_t>(direct_cli->run({"project", "create", "--help"}, null_out, null_err));
    });

    std::cout << '\n' << "sink=" << g_sink << '\n';
    return 0;
}
