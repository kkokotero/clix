#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <clix/cli.hpp>
#include <clix/completion.hpp>
#include <clix/config.hpp>
#include <clix/platform.hpp>
#include <clix/router.hpp>
#include <clix/validators.hpp>

namespace fs = std::filesystem;

namespace {

struct RunResult {
    int exit_code = 0;
    std::string out;
    std::string err;
};

struct DemoCapture {
    int create_calls = 0;
    int inspect_calls = 0;
    std::vector<std::string> command_path;
    std::string name;
    std::string template_name;
    std::string language;
    double jobs = 0.0;
    std::string format;
    std::string region;
    std::vector<std::string> tags;
    std::string output;
    std::string manifest;
    bool verbose = false;
    std::vector<std::string> passthrough;
};

struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;

    [[nodiscard]] std::string to_string() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    [[nodiscard]] bool operator==(const SemanticVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }
};

class TempDirectory {
public:
    TempDirectory()
        : path_(fs::temp_directory_path() /
                ("clix-tests-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))) {
        fs::create_directories(path_);
    }

    ~TempDirectory() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

class ScopedEnvVar {
public:
    ScopedEnvVar(std::string name, std::string value)
        : name_(std::move(name))
        , had_value_(read(name_).has_value())
        , previous_value_(read(name_)) {
        write(name_, value);
    }

    ~ScopedEnvVar() {
        if (had_value_ && previous_value_.has_value()) {
            write(name_, *previous_value_);
        } else {
            erase(name_);
        }
    }

private:
    [[nodiscard]] static std::optional<std::string> read(const std::string& name) {
        const auto* value = std::getenv(name.c_str());
        if (value == nullptr) {
            return std::nullopt;
        }
        return std::string(value);
    }

    static void write(const std::string& name, const std::string& value) {
#if defined(_WIN32)
        _putenv_s(name.c_str(), value.c_str());
#else
        setenv(name.c_str(), value.c_str(), 1);
#endif
    }

    static void erase(const std::string& name) {
#if defined(_WIN32)
        _putenv_s(name.c_str(), "");
#else
        unsetenv(name.c_str());
#endif
    }

    std::string name_;
    bool had_value_ = false;
    std::optional<std::string> previous_value_;
};

[[nodiscard]] clix::ArgumentConfig make_argument_config(clix::ValueKind kind,
                                                        std::string description,
                                                        std::string value_label,
                                                        bool optional = false) {
    clix::ArgumentConfig config;
    config.kind = kind;
    config.description = std::move(description);
    config.value_label = std::move(value_label);
    config.optional = optional;
    return config;
}

[[nodiscard]] clix::OptionConfig make_option_config(clix::ValueKind kind,
                                                    std::string description,
                                                    std::string value_label,
                                                    std::string group = "Options",
                                                    bool optional = true) {
    clix::OptionConfig config;
    config.kind = kind;
    config.description = std::move(description);
    config.value_label = std::move(value_label);
    config.group = std::move(group);
    config.optional = optional;
    return config;
}

[[nodiscard]] clix::ConfigFileSettings make_demo_config_settings() {
    clix::ConfigFileSettings settings;
    settings.environment_variables = {"DEMO_CONFIG"};
    settings.default_filenames = {"demo.toml", "demo.json"};
    return settings;
}

[[nodiscard]] std::unique_ptr<clix::CLI> make_demo_cli(DemoCapture& capture) {
    auto cli = std::make_unique<clix::CLI>("demo", "1.2.3");
    cli->description("Integration fixture used by the test suite.");
    cli->enable_completion();
    cli->enable_config_files(make_demo_config_settings());
    cli->opt("verbose")
        .alias("V")
        .description("Enable verbose output.");

    auto& project = cli->command("project").description("Project lifecycle commands.");

    auto& create = project.command("create").description("Create a project from a starter template.");

    auto name_argument = make_argument_config(clix::ValueKind::string, "Project name.", "name");
    name_argument.environment_variables = {"DEMO_NAME"};
    name_argument.validators.push_back(clix::validators::non_empty_string());
    create.argument("name", std::move(name_argument));

    auto template_argument =
        make_argument_config(clix::ValueKind::choice, "Starter template.", "template", true);
    template_argument.default_value = clix::CliValue(std::string("app"));
    template_argument.choices = {"app", "lib", "tool"};
    create.argument("template", std::move(template_argument));

    auto project_group = create.option_group("Project", "Project generation settings.");

    auto language_option =
        make_option_config(clix::ValueKind::choice, "Target implementation language.", "language", "Project");
    language_option.aliases = {"l"};
    language_option.environment_variables = {"DEMO_LANGUAGE"};
    language_option.default_value = clix::CliValue(std::string("cpp"));
    language_option.choices = {"cpp", "ts"};
    project_group.option("language", std::move(language_option));

    auto jobs_option = make_option_config(clix::ValueKind::number, "Parallel jobs.", "count", "Project");
    jobs_option.aliases = {"j"};
    jobs_option.environment_variables = {"DEMO_JOBS"};
    jobs_option.default_value = clix::CliValue(1.0);
    jobs_option.validators.push_back(clix::validators::number_range(1.0, 32.0));
    project_group.option("jobs", std::move(jobs_option));

    auto region_option = make_option_config(clix::ValueKind::string, "Target region.", "region", "Project");
    region_option.completion_provider = [](std::string_view) {
        return std::vector<clix::CompletionSuggestion>{{"us-east-1", "AWS US East"},
                                                       {"eu-west-1", "AWS EU West"},
                                                       {"ap-southeast-1", "AWS AP Southeast"}};
    };
    project_group.option("region", std::move(region_option));

    auto output_option = make_option_config(clix::ValueKind::path, "Output directory.", "path", "Project");
    output_option.aliases = {"o"};
    project_group.option("output", std::move(output_option));

    auto output_group = create.option_group("Output", "Output rendering settings.");

    auto format_option = make_option_config(clix::ValueKind::choice, "Serialization format.", "format", "Output");
    format_option.aliases = {"f"};
    format_option.default_value = clix::CliValue(std::string("json"));
    format_option.choices = {"json", "yaml", "toml"};
    output_group.option("format", std::move(format_option));

    auto tag_option = make_option_config(clix::ValueKind::string_array, "Project tags.", "tag", "Output");
    tag_option.completion_values = {{"core", "tag"}, {"cli", "tag"}, {"web", "tag"}};
    output_group.option("tag", std::move(tag_option));

    create.action([&capture](const clix::Invocation& invocation) {
        ++capture.create_calls;
        capture.command_path = invocation.command_path();
        capture.name = invocation.argument<std::string>("name");
        capture.template_name = invocation.argument<std::string>("template");
        capture.language = invocation.option<std::string>("language");
        capture.jobs = invocation.option<double>("jobs");
        capture.format = invocation.option<std::string>("format");
        capture.region = invocation.option_or<std::string>("region", "");
        capture.output = invocation.has_option("output") ? invocation.option<clix::Path>("output").to_string() : "";
        capture.tags = invocation.has_option("tag") ? invocation.option<std::vector<std::string>>("tag")
                                                     : std::vector<std::string>{};
        capture.verbose = invocation.option<bool>("verbose");
        capture.passthrough = invocation.passthrough_tokens();
    });

    auto& inspect = project.command("inspect").description("Inspect an existing project manifest.");
    auto manifest_argument = make_argument_config(clix::ValueKind::path, "Project manifest path.", "path");
    manifest_argument.validators.push_back(clix::validators::existing_path());
    inspect.argument("manifest", std::move(manifest_argument));
    inspect.action([&capture](const clix::Invocation& invocation) {
        ++capture.inspect_calls;
        capture.manifest = invocation.argument<clix::Path>("manifest").to_string();
    });

    return cli;
}

[[nodiscard]] RunResult run_cli(const clix::CLI& cli, std::vector<std::string> argv) {
    std::ostringstream out;
    std::ostringstream err;
    const auto exit_code = cli.run(argv, out, err);
    return RunResult{exit_code, out.str(), err.str()};
}

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename T>
void expect_equal(const T& actual, const T& expected, std::string_view label) {
    if (!(actual == expected)) {
        throw std::runtime_error(std::string(label) + ": values differ");
    }
}

[[nodiscard]] SemanticVersion parse_semantic_version(std::string_view raw) {
    const auto parts = clix::detail::split(raw, '.');
    if (parts.size() != 3) {
        throw clix::ParsingError(
            "Invalid semantic version",
            clix::ParsingErrorOptions{{}, {std::string(raw)}, {"1.2.3"}, "Use the MAJOR.MINOR.PATCH format."});
    }

    try {
        return SemanticVersion{
            std::stoi(parts[0]),
            std::stoi(parts[1]),
            std::stoi(parts[2]),
        };
    } catch (const std::exception&) {
        throw clix::ParsingError(
            "Invalid semantic version",
            clix::ParsingErrorOptions{{}, {std::string(raw)}, {"1.2.3"}, "Use numeric semantic version segments."});
    }
}

void expect_contains(std::string_view text, std::string_view needle, std::string_view label) {
    if (text.find(needle) == std::string_view::npos) {
        throw std::runtime_error(std::string(label) + ": expected to find [" + std::string(needle) +
                                 "] in [" + std::string(text) + "]");
    }
}

void expect_not_contains(std::string_view text, std::string_view needle, std::string_view label) {
    if (text.find(needle) != std::string_view::npos) {
        throw std::runtime_error(std::string(label) + ": did not expect to find [" + std::string(needle) +
                                 "] in [" + std::string(text) + "]");
    }
}

[[nodiscard]] bool contains_value(const std::vector<clix::CompletionSuggestion>& suggestions,
                                  std::string_view value) {
    return std::any_of(suggestions.begin(), suggestions.end(), [value](const clix::CompletionSuggestion& item) {
        return item.value == value;
    });
}

void test_help_and_version_exit_success() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto help_result = run_cli(*cli, {"project", "create", "--help"});
    expect_equal(help_result.exit_code, 0, "help exit code");
    expect_contains(help_result.out, "Project generation settings.", "help output includes project group description");
    expect_contains(help_result.out, "Output:", "help output includes output group");
    expect(help_result.err.empty(), "help should not write to stderr");

    const auto version_result = run_cli(*cli, {"project", "create", "--version"});
    expect_equal(version_result.exit_code, 0, "version exit code");
    expect_contains(version_result.out, "demo v1.2.3", "version output");
    expect(version_result.err.empty(), "version should not write to stderr");
}

void test_nested_subcommand_invocation_and_defaults() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "widget"});
    expect_equal(result.exit_code, 0, "create exit code");
    expect(result.err.empty(), "create should not write to stderr");
    expect_equal(capture.create_calls, 1, "create handler call count");
    expect_equal(capture.command_path, std::vector<std::string>({"demo", "project", "create"}), "command path");
    expect_equal(capture.name, std::string("widget"), "captured project name");
    expect_equal(capture.template_name, std::string("app"), "default template");
    expect_equal(capture.language, std::string("cpp"), "default language");
    expect_equal(capture.jobs, 1.0, "default jobs");
    expect_equal(capture.format, std::string("json"), "default format");
}

void test_validators_reject_invalid_numeric_values() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "widget", "--jobs", "0"});
    expect_equal(result.exit_code, 1, "invalid jobs exit code");
    expect_contains(result.err, "Validation failed", "validator error header");
    expect_contains(result.err, "number_range", "validator name");
}

void test_existing_path_validator_accepts_and_rejects() {
    TempDirectory temp_directory;
    const auto manifest_path = temp_directory.path() / "project.json";
    std::ofstream(manifest_path) << "{}";

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto ok_result = run_cli(*cli, {"project", "inspect", manifest_path.string()});
    expect_equal(ok_result.exit_code, 0, "inspect exit code");
    expect_equal(capture.inspect_calls, 1, "inspect handler call count");
    expect_equal(capture.manifest, fs::absolute(manifest_path).lexically_normal().string(), "captured manifest path");

    const auto missing_result =
        run_cli(*cli, {"project", "inspect", (temp_directory.path() / "missing.json").string()});
    expect_equal(missing_result.exit_code, 1, "inspect missing path exit code");
    expect_contains(missing_result.err, "Path must exist.", "missing path validator message");
}

void test_config_file_supplies_missing_values_and_merges_sections() {
    TempDirectory temp_directory;
    const auto config_path = temp_directory.path() / "demo.toml";

    std::ofstream stream(config_path);
    stream << "language = \"ts\"\n";
    stream << "format = \"yaml\"\n";
    stream << "[project.create]\n";
    stream << "name = \"from-config\"\n";
    stream << "jobs = 4\n";
    stream << "region = \"us-east-1\"\n";
    stream << "tag = [\"core\", \"cli\"]\n";
    stream.close();

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "--config", config_path.string()});
    expect_equal(result.exit_code, 0, "config-backed create exit code");
    expect_equal(capture.name, std::string("from-config"), "config supplied name");
    expect_equal(capture.language, std::string("ts"), "root section language");
    expect_equal(capture.jobs, 4.0, "section jobs");
    expect_equal(capture.format, std::string("yaml"), "root section format");
    expect_equal(capture.region, std::string("us-east-1"), "section region");
    expect_equal(capture.tags, std::vector<std::string>({"core", "cli"}), "section tags");
}

void test_command_line_values_override_config_values() {
    TempDirectory temp_directory;
    const auto config_stem = temp_directory.path() / "override";
    const auto config_path = config_stem.string() + ".json";

    std::ofstream stream(config_path);
    stream << "{\n";
    stream << "  \"language\": \"ts\",\n";
    stream << "  \"project\": {\n";
    stream << "    \"create\": {\n";
    stream << "      \"name\": \"from-config\"\n";
    stream << "    }\n";
    stream << "  }\n";
    stream << "}\n";
    stream.close();

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(
        *cli,
        {"project", "create", "from-cli", "--config", config_stem.string(), "--language", "cpp"});
    expect_equal(result.exit_code, 0, "override create exit code");
    expect_equal(capture.name, std::string("from-cli"), "command line name wins");
    expect_equal(capture.language, std::string("cpp"), "command line option wins");
}

void test_strict_config_rejects_unknown_keys() {
    TempDirectory temp_directory;
    const auto config_path = temp_directory.path() / "strict.toml";

    std::ofstream stream(config_path);
    stream << "[project.create]\n";
    stream << "name = \"widget\"\n";
    stream << "unknown = \"value\"\n";
    stream.close();

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "--config", config_path.string()});
    expect_equal(result.exit_code, 1, "strict config exit code");
    expect_contains(result.err, "Unknown config key", "strict config message");
}

void test_root_completion_includes_commands_and_builtin_options() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto suggestions = cli->complete({});
    expect(contains_value(suggestions, "project"), "root completion should include subcommands");
    expect(contains_value(suggestions, "--help"), "root completion should include --help");
    expect(contains_value(suggestions, "--generate-completion"), "root completion should include completion option");
    expect(contains_value(suggestions, "--config"), "root completion should include config option");
}

void test_nested_subcommand_and_option_value_completion() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto command_suggestions = cli->complete({"project", "cr"});
    expect(contains_value(command_suggestions, "create"), "nested command completion should include create");

    const auto language_suggestions = cli->complete({"project", "create", "--language", ""});
    expect(contains_value(language_suggestions, "cpp"), "language completion should include cpp");
    expect(contains_value(language_suggestions, "ts"), "language completion should include ts");

    const auto tag_suggestions = cli->complete({"project", "create", "--tag", "c"});
    expect(contains_value(tag_suggestions, "core"), "tag completion should include core");
    expect(contains_value(tag_suggestions, "cli"), "tag completion should include cli");

    const auto region_suggestions = cli->complete({"project", "create", "--region", "us"});
    expect(contains_value(region_suggestions, "us-east-1"), "region completion should include provider values");
}

void test_hidden_completion_backend_writes_lines() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"__clix_complete", "bash", "project", "create", "--language", ""});
    expect_equal(result.exit_code, 0, "hidden completion exit code");
    expect_contains(result.out, "cpp", "hidden completion output contains cpp");
    expect_contains(result.out, "ts", "hidden completion output contains ts");
}

void test_completion_script_generation_supports_all_shells() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    expect_contains(cli->completion_script(clix::CompletionShell::bash, "demo"),
                    "complete -F",
                    "bash completion script");
    expect_contains(cli->completion_script(clix::CompletionShell::zsh, "demo"),
                    "#compdef demo",
                    "zsh completion script");
    expect_contains(cli->completion_script(clix::CompletionShell::fish, "demo"),
                    "complete -c demo",
                    "fish completion script");
    expect_contains(cli->completion_script(clix::CompletionShell::powershell, "demo"),
                    "Register-ArgumentCompleter",
                    "powershell completion script");

    const auto result = run_cli(*cli, {"--generate-completion", "bash"});
    expect_equal(result.exit_code, 0, "completion generation exit code");
    expect_contains(result.out, "__clix_complete", "generated completion uses hidden command");
}

void test_option_groups_appear_in_help_output() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "--help"});
    expect_equal(result.exit_code, 0, "group help exit code");
    expect_contains(result.out, "Project generation settings.", "project group description");
    expect_contains(result.out, "Output rendering settings.", "output group description");
    expect_contains(result.out, "Project:", "project group header");
    expect_contains(result.out, "Output:", "output group header");
}

void test_path_completion_suggests_filesystem_entries() {
    TempDirectory temp_directory;
    const auto manifest_path = temp_directory.path() / "manifest.json";
    std::ofstream(manifest_path) << "{}";

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto suggestions = cli->complete({"project", "inspect", (temp_directory.path() / "man").string()});
    expect(contains_value(suggestions, manifest_path.string()), "path completion should include matching file");
}

void test_global_options_before_subcommand_are_supported() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"--verbose", "project", "create", "widget"});
    expect_equal(result.exit_code, 0, "global option before subcommand exit code");
    expect(capture.verbose, "global option should be visible from the nested command");
    expect_equal(capture.name, std::string("widget"), "nested command should still parse arguments");

    const auto suggestions = cli->complete({"--verbose", "project", "cr"});
    expect(contains_value(suggestions, "create"), "completion should still see subcommands after global options");
}

void test_config_values_have_higher_precedence_than_environment() {
    TempDirectory temp_directory;
    const auto config_stem = temp_directory.path() / "config-priority";
    const auto config_path = config_stem.string() + ".json";

    std::ofstream stream(config_path);
    stream << "{\n";
    stream << "  \"language\": \"cpp\",\n";
    stream << "  \"project\": {\n";
    stream << "    \"create\": {\n";
    stream << "      \"name\": \"from-config\",\n";
    stream << "      \"jobs\": 2\n";
    stream << "    }\n";
    stream << "  }\n";
    stream << "}\n";
    stream.close();

    ScopedEnvVar env_name("DEMO_NAME", "from-env");
    ScopedEnvVar env_language("DEMO_LANGUAGE", "ts");
    ScopedEnvVar env_jobs("DEMO_JOBS", "7");

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "--config", config_stem.string()});
    expect_equal(result.exit_code, 0, "config precedence exit code");
    expect_equal(capture.name, std::string("from-config"), "config should override environment for arguments");
    expect_equal(capture.language, std::string("cpp"), "config should override environment for options");
    expect_equal(capture.jobs, 2.0, "config numeric value should override environment");
}

void test_config_path_can_come_from_environment_or_default_filename() {
    TempDirectory temp_directory;
    const auto env_config_stem = temp_directory.path() / "from-env";
    const auto default_config_stem = temp_directory.path() / "demo";
    const auto env_config_path = env_config_stem.string() + ".toml";
    const auto default_config_path = default_config_stem.string() + ".json";

    {
        std::ofstream stream(env_config_path);
        stream << "[project.create]\n";
        stream << "name = \"from-env-config\"\n";
    }
    {
        std::ofstream stream(default_config_path);
        stream << "{\n";
        stream << "  \"project\": {\n";
        stream << "    \"create\": {\n";
        stream << "      \"name\": \"from-default-file\"\n";
        stream << "    }\n";
        stream << "  }\n";
        stream << "}\n";
    }

    DemoCapture env_capture;
    auto env_cli = make_demo_cli(env_capture);
    env_cli->environment_reader(
        [env_config_stem](std::string_view name) -> std::optional<std::string> {
            if (name == "DEMO_CONFIG") {
                return env_config_stem.string();
            }
            return std::nullopt;
        });

    const auto env_result = run_cli(*env_cli, {"project", "create"});
    expect_equal(env_result.exit_code, 0, "config from environment exit code");
    expect_equal(env_capture.name, std::string("from-env-config"), "config path should be resolved from environment");

    DemoCapture default_capture;
    auto default_cli = make_demo_cli(default_capture);
    auto settings = make_demo_config_settings();
    settings.environment_variables.clear();
    settings.default_filenames = {default_config_stem.string()};

    default_cli = std::make_unique<clix::CLI>("demo", "1.2.3");
    default_cli->description("Integration fixture used by the test suite.");
    default_cli->enable_completion();
    default_cli->enable_config_files(settings);
    default_cli->opt("verbose")
        .alias("V")
        .description("Enable verbose output.");

    auto& project = default_cli->command("project").description("Project lifecycle commands.");
    auto& create = project.command("create").description("Create a project from a starter template.");

    auto name_argument = make_argument_config(clix::ValueKind::string, "Project name.", "name");
    create.argument("name", std::move(name_argument));
    create.action([&default_capture](const clix::Invocation& invocation) {
        default_capture.name = invocation.argument<std::string>("name");
    });

    const auto default_result = run_cli(*default_cli, {"project", "create"});
    expect_equal(default_result.exit_code, 0, "config from default filename exit code");
    expect_equal(default_capture.name,
                 std::string("from-default-file"),
                 "config path should be resolved from default filenames");
}

void test_custom_environment_reader_supplies_declared_values() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    cli->environment_reader([](std::string_view name) -> std::optional<std::string> {
        if (name == "DEMO_NAME") {
            return std::string("reader-name");
        }
        if (name == "DEMO_LANGUAGE") {
            return std::string("ts");
        }
        if (name == "DEMO_JOBS") {
            return std::string("5");
        }
        return std::nullopt;
    });

    const auto result = run_cli(*cli, {"project", "create"});
    expect_equal(result.exit_code, 0, "custom environment reader exit code");
    expect_equal(capture.name, std::string("reader-name"), "custom reader should resolve argument values");
    expect_equal(capture.language, std::string("ts"), "custom reader should resolve option values");
    expect_equal(capture.jobs, 5.0, "custom reader should resolve numeric option values");
}

void test_process_environment_is_used_even_with_custom_reader() {
    ScopedEnvVar env_name("DEMO_NAME", "from-process-env");

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    cli->environment_reader([](std::string_view name) -> std::optional<std::string> {
        if (name == "UNUSED_NAME") {
            return std::string("never-used");
        }
        return std::nullopt;
    });

    const auto result = run_cli(*cli, {"project", "create"});
    expect_equal(result.exit_code, 0, "process environment fallback exit code");
    expect_equal(capture.name,
                 std::string("from-process-env"),
                 "process environment should still be consulted when the custom reader has no value");
}

void test_unsupported_config_extensions_are_rejected() {
    TempDirectory temp_directory;
    const auto config_path = temp_directory.path() / "invalid.yaml";

    std::ofstream stream(config_path);
    stream << "name: invalid\n";
    stream.close();

    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "--config", config_path.string()});
    expect_equal(result.exit_code, 1, "unsupported extension exit code");
    expect_contains(result.err, "Unsupported config file extension", "unsupported extension message");
}

void test_config_extension_order_is_respected_for_extensionless_paths() {
    TempDirectory temp_directory;
    const auto config_stem = temp_directory.path() / "ordered-config";
    const auto json_path = config_stem.string() + ".json";
    const auto toml_path = config_stem.string() + ".toml";

    {
        std::ofstream stream(json_path);
        stream << "{\n";
        stream << "  \"project\": {\n";
        stream << "    \"create\": {\n";
        stream << "      \"name\": \"from-json\"\n";
        stream << "    }\n";
        stream << "  }\n";
        stream << "}\n";
    }
    {
        std::ofstream stream(toml_path);
        stream << "[project.create]\n";
        stream << "name = \"from-toml\"\n";
    }

    DemoCapture capture;
    auto cli = make_demo_cli(capture);
    auto settings = make_demo_config_settings();
    settings.allowed_extensions = {".json", ".toml"};

    cli = std::make_unique<clix::CLI>("demo", "1.2.3");
    cli->description("Integration fixture used by the test suite.");
    cli->enable_completion();
    cli->enable_config_files(settings);

    auto& project = cli->command("project").description("Project lifecycle commands.");
    auto& create = project.command("create").description("Create a project from a starter template.");
    auto name_argument = make_argument_config(clix::ValueKind::string, "Project name.", "name");
    create.argument("name", std::move(name_argument));
    create.action([&capture](const clix::Invocation& invocation) {
        capture.name = invocation.argument<std::string>("name");
    });

    const auto result = run_cli(*cli, {"project", "create", "--config", config_stem.string()});
    expect_equal(result.exit_code, 0, "extension order exit code");
    expect_equal(capture.name, std::string("from-json"), "allowed_extensions should control probing order");
}

void test_end_of_flags_marker_requires_following_values() {
    DemoCapture capture;
    auto cli = make_demo_cli(capture);

    const auto result = run_cli(*cli, {"project", "create", "--"});
    expect_equal(result.exit_code, 1, "end-of-flags error exit code");
    expect_contains(result.err, "Unexpected end-of-options marker", "end-of-flags error message");
}

void test_short_option_groups_support_inline_value_suffixes() {
    bool verbose = false;
    double jobs = 0.0;

    clix::CLI cli("tool");
    auto& build = cli.command("build").description("Build the current project.");
    build.opt("verbose").alias("v").description("Enable verbose output.");
    build.opt("jobs", clix::ValueKind::number).alias("j").description("Parallel jobs.");
    build.action([&](const clix::Invocation& invocation) {
        verbose = invocation.option<bool>("verbose");
        jobs = invocation.option<double>("jobs");
    });

    const auto result = run_cli(cli, {"build", "-vj4"});
    expect_equal(result.exit_code, 0, "inline short option suffix exit code");
    expect(verbose, "grouped short options should set boolean values");
    expect_equal(jobs, 4.0, "grouped short options should parse inline values");
}

void test_requires_excludes_and_exclusive_groups_are_enforced() {
    clix::CLI cli("release");
    auto& publish = cli.command("publish").description("Publish a release artifact.");
    publish.opt("token", clix::ValueKind::string).description("Authentication token.");
    publish.opt("push").description("Push the release.").requires("token");
    publish.opt("dry-run").description("Preview the release.").excludes("push");
    publish.opt("json").description("Render JSON output.").exclusive_group("output");
    publish.opt("yaml").description("Render YAML output.").exclusive_group("output");
    publish.action([](const clix::Invocation&) {});

    const auto missing_required = run_cli(cli, {"publish", "--push"});
    expect_equal(missing_required.exit_code, 1, "requires option exit code");
    expect_contains(missing_required.err, "requires --token", "requires option message");

    const auto excluded = run_cli(cli, {"publish", "--push", "--token", "secret", "--dry-run"});
    expect_equal(excluded.exit_code, 1, "excludes option exit code");
    expect_contains(excluded.err, "cannot be used together", "excludes option message");

    const auto exclusive = run_cli(cli, {"publish", "--json", "--yaml"});
    expect_equal(exclusive.exit_code, 1, "exclusive group exit code");
    expect_contains(exclusive.err, "Mutually exclusive options", "exclusive group message");
}

void test_passthrough_collects_unknown_options_and_extra_arguments() {
    std::vector<std::string> passthrough_tokens;

    clix::CLI cli("runner");
    auto& run = cli.command("run").allow_passthrough().description("Execute a command.");
    run.action([&](const clix::Invocation& invocation) {
        passthrough_tokens = invocation.passthrough_tokens();
    });

    const auto result = run_cli(cli, {"run", "--engine", "v8", "script.ts", "--watch"});
    expect_equal(result.exit_code, 0, "passthrough exit code");
    expect_equal(passthrough_tokens,
                 std::vector<std::string>({"--engine", "v8", "script.ts", "--watch"}),
                 "passthrough tokens");
}

void test_hidden_options_are_runtime_only() {
    bool internal = false;

    clix::CLI cli("internal");
    auto& run = cli.command("run").description("Run the internal workflow.");
    run.opt("internal").description("Internal mode.").hidden();
    run.action([&](const clix::Invocation& invocation) {
        internal = invocation.option<bool>("internal");
    });

    const auto help_result = run_cli(cli, {"run", "--help"});
    expect_equal(help_result.exit_code, 0, "hidden help exit code");
    expect_not_contains(help_result.out, "--internal", "hidden options should not appear in help");

    const auto suggestions = cli.complete({"run", "--"});
    expect(!contains_value(suggestions, "--internal"), "hidden options should not appear in completion");

    const auto run_result = run_cli(cli, {"run", "--internal"});
    expect_equal(run_result.exit_code, 0, "hidden runtime option exit code");
    expect(internal, "hidden options should still be parsed at runtime");
}

void test_child_commands_override_parent_option_definitions() {
    std::string profile;

    clix::CLI cli("override");
    cli.opt("profile", clix::ValueKind::choice)
        .description("Root execution profile.")
        .choices({"root"})
        .default_value(clix::CliValue(std::string("root")));

    auto& child = cli.command("child").description("Child command.");
    child.opt("profile", clix::ValueKind::choice)
        .description("Child execution profile.")
        .choices({"debug", "release"})
        .default_value(clix::CliValue(std::string("release")));
    child.action([&](const clix::Invocation& invocation) {
        profile = invocation.option<std::string>("profile");
    });

    const auto result = run_cli(cli, {"child"});
    expect_equal(result.exit_code, 0, "child override exit code");
    expect_equal(profile, std::string("release"), "child defaults should override parent defaults");

    const auto completion = cli.complete({"child", "--profile", ""});
    expect(contains_value(completion, "debug"), "child completion should include child-only values");
    expect(!contains_value(completion, "root"), "child completion should not leak parent choices");
}

void test_router_mounts_modular_command_trees() {
    bool verbose = false;
    std::string created_name;
    std::string inspected_manifest;

    clix::Router project_router;
    project_router.root([](clix::Command& project) {
        project.description("Project lifecycle commands.");
    });
    project_router.command("create", [&](clix::Command& create) {
        create.description("Create a new project.");
        create.arg("name").description("Project name.").label("name");
        create.action([&](const clix::Invocation& invocation) {
            created_name = invocation.argument<std::string>("name");
            verbose = invocation.option<bool>("verbose");
        });
    });

    clix::Router inspect_router;
    inspect_router.command("inspect", [&](clix::Command& inspect) {
        inspect.description("Inspect an existing project.");
        inspect.arg("manifest", clix::ValueKind::path).description("Project manifest.");
        inspect.action([&](const clix::Invocation& invocation) {
            inspected_manifest = invocation.argument<clix::Path>("manifest").to_string();
        });
    });

    project_router.use(inspect_router);

    clix::Router app_router;
    app_router.root([](clix::Command& root) {
        root.description("Application configured through routers.");
        root.opt("verbose").description("Enable verbose output.");
    });
    app_router.use("project", project_router);

    clix::CLI cli("router-demo");
    app_router.mount(cli);

    const auto create_result = run_cli(cli, {"--verbose", "project", "create", "widget"});
    expect_equal(create_result.exit_code, 0, "router create exit code");
    expect(verbose, "router should preserve inherited root options");
    expect_equal(created_name, std::string("widget"), "router should mount nested create command");

    const auto inspect_result = run_cli(cli, {"project", "inspect", "manifest.json"});
    expect_equal(inspect_result.exit_code, 0, "router inspect exit code");
    expect_contains(inspected_manifest, "manifest.json", "router should mount merged child routers");

    const auto help_result = run_cli(cli, {"project", "--help"});
    expect_equal(help_result.exit_code, 0, "router help exit code");
    expect_contains(help_result.out, "Project lifecycle commands.", "router root config should apply to mounted scope");
}

void test_router_reuses_existing_paths_and_rejects_invalid_routes() {
    std::vector<std::string> collected;

    clix::Router router;
    router.command("workspace/run", [](clix::Command& run) {
        run.description("Run a workspace task.");
    });
    router.command("workspace/run", [&](clix::Command& run) {
        run.opt("tag", clix::ValueKind::string).description("Task tag.");
        run.action([&](const clix::Invocation& invocation) {
            collected.push_back(invocation.option_or<std::string>("tag", ""));
        });
    });

    clix::CLI cli("workspace");
    router.mount(cli);

    const auto result = run_cli(cli, {"workspace", "run", "--tag", "lint"});
    expect_equal(result.exit_code, 0, "router duplicate path exit code");
    expect_equal(collected, std::vector<std::string>({"lint"}), "router should augment existing commands");

    bool threw = false;
    try {
        clix::Router invalid;
        invalid.command("workspace//run", [](clix::Command&) {});
    } catch (const clix::CommandError&) {
        threw = true;
    }
    expect(threw, "router should reject invalid empty path segments");
}

void test_url_value_kind_parses_absolute_urls() {
    clix::Url captured("https://placeholder.dev");

    clix::CLI cli("fetch");
    auto& get = cli.command("get").description("Fetch a remote resource.");
    get.arg("endpoint", clix::ValueKind::url).description("Absolute endpoint URL.");
    get.action([&](const clix::Invocation& invocation) {
        captured = invocation.argument<clix::Url>("endpoint");
    });

    const auto ok = run_cli(cli, {"get", "https://example.com/api/items?id=42#top"});
    expect_equal(ok.exit_code, 0, "url parse exit code");
    expect_equal(captured.scheme(), std::string("https"), "url scheme");
    expect_equal(captured.authority(), std::string("example.com"), "url authority");
    expect_equal(captured.path(), std::string("/api/items"), "url path");
    expect_equal(captured.query(), std::string("id=42"), "url query");
    expect_equal(captured.fragment(), std::string("top"), "url fragment");

    const auto invalid = run_cli(cli, {"get", "example.com/no-scheme"});
    expect_equal(invalid.exit_code, 1, "invalid url exit code");
    expect_contains(invalid.err, "URL is missing a scheme", "invalid url error");
}

void test_custom_typed_parsers_and_value_sources_are_available_in_invocations() {
    TempDirectory temp_directory;
    const auto config_path = temp_directory.path() / "source-demo.toml";

    {
        std::ofstream stream(config_path);
        stream << "[release.publish]\n";
        stream << "profile = \"release\"\n";
    }

    ScopedEnvVar config_env("RELEASE_CONFIG", config_path.string());
    ScopedEnvVar target_env("RELEASE_TARGET", "linux-arm64");

    SemanticVersion version;
    clix::ValueSource version_source = clix::ValueSource::default_value;
    clix::ValueSource target_source = clix::ValueSource::default_value;
    clix::ValueSource profile_source = clix::ValueSource::default_value;
    clix::ValueSource format_source = clix::ValueSource::default_value;

    clix::ConfigFileSettings settings;
    settings.environment_variables = {"RELEASE_CONFIG"};

    clix::CLI cli("release-cli");
    cli.enable_config_files(settings);

    auto& release = cli.command("release").description("Release workflows.");
    auto& publish = release.command("publish").description("Publish a release.");
    publish.arg("version")
        .description("Release semantic version.")
        .parse_as<SemanticVersion>(
            [](std::string_view raw) { return parse_semantic_version(raw); },
            "semver",
            [](const SemanticVersion& value) { return value.to_string(); });
    publish.opt("target", clix::ValueKind::string)
        .description("Deployment target.")
        .env("RELEASE_TARGET");
    publish.opt("profile", clix::ValueKind::string)
        .description("Release profile.");
    publish.opt("format", clix::ValueKind::string)
        .description("Output format.")
        .default_value(clix::CliValue(std::string("text")));
    publish.action([&](const clix::Invocation& invocation) {
        version = invocation.argument<SemanticVersion>("version");
        version_source = invocation.argument_source("version");
        target_source = invocation.option_source("target");
        profile_source = invocation.option_source("profile");
        format_source = invocation.source("format");
    });

    const auto result = run_cli(cli, {"release", "publish", "2.4.6"});
    expect_equal(result.exit_code, 0, "custom parser exit code");
    expect_equal(version, SemanticVersion{2, 4, 6}, "custom parsed semantic version");
    expect_equal(version_source, clix::ValueSource::command_line, "argument source should be command line");
    expect_equal(target_source, clix::ValueSource::environment, "option source should be environment");
    expect_equal(profile_source, clix::ValueSource::config_file, "option source should be config");
    expect_equal(format_source, clix::ValueSource::default_value, "default option source");
}

void test_validator_composition_helpers_cover_common_constraints() {
    clix::CLI cli("validate");
    auto& run = cli.command("run").description("Validate composed validators.");
    run.arg("name")
        .validate(clix::validators::all_of(
            {clix::validators::non_empty_string(),
             clix::validators::length(3, 8),
             clix::validators::matches("^[a-z]+$")}))
        .description("Lowercase project name.");
    run.opt("mode", clix::ValueKind::string)
        .choices({"dev", "prod"})
        .validate(clix::validators::any_of(
            {clix::validators::matches("^dev$"), clix::validators::matches("^prod$")}));
    run.opt("reserved", clix::ValueKind::string)
        .validate(clix::validators::none_of(
            {clix::validators::matches("^root$"), clix::validators::matches("^admin$")}));
    run.action([](const clix::Invocation&) {});

    const auto invalid_name = run_cli(cli, {"run", "Ab", "--mode", "dev", "--reserved", "guest"});
    expect_equal(invalid_name.exit_code, 1, "composed validator invalid-name exit code");
    expect_contains(invalid_name.err, "Validation failed", "composed validator failure");

    const auto invalid_reserved = run_cli(cli, {"run", "widget", "--mode", "prod", "--reserved", "root"});
    expect_equal(invalid_reserved.exit_code, 1, "composed validator invalid-reserved exit code");
    expect_contains(invalid_reserved.err, "none_of", "none_of validator name");

    const auto ok = run_cli(cli, {"run", "widget", "--mode", "dev", "--reserved", "guest"});
    expect_equal(ok.exit_code, 0, "composed validator success exit code");
}

void test_command_bundles_improve_reuse_without_duplicating_schema() {
    bool verbose = false;
    bool json = false;

    auto logging_bundle = [](clix::Command& command) {
        command.opt("verbose").alias("L").description("Enable verbose output.");
        command.opt("json").description("Render machine-readable JSON.");
    };

    clix::CLI cli("bundle");
    auto& run = cli.command("run").description("Run the task.").use(logging_bundle);
    run.action([&](const clix::Invocation& invocation) {
        verbose = invocation.option<bool>("verbose");
        json = invocation.option<bool>("json");
    });

    const auto result = run_cli(cli, {"run", "-L", "--json"});
    expect_equal(result.exit_code, 0, "bundle exit code");
    expect(verbose, "bundle should expose verbose option");
    expect(json, "bundle should expose json option");
}

void test_deprecated_commands_options_schema_and_markdown_are_exposed() {
    bool invoked = false;

    clix::CLI cli("legacy");
    auto& deploy = cli.command("deploy")
                       .description("Deploy the service.")
                       .deprecated("Use `release deploy` instead.")
                       .alias("push")
                       .deprecated_alias("ship", "Use `deploy` instead.");
    deploy.opt("token", clix::ValueKind::string)
        .description("Authentication token.")
        .deprecated("Use `api-token` instead.")
        .deprecated_alias("t", "Use `--token` instead.");
    deploy.action([&](const clix::Invocation&) {
        invoked = true;
    });

    const auto result = run_cli(cli, {"ship", "-t", "secret"});
    expect_equal(result.exit_code, 0, "deprecated command exit code");
    expect(invoked, "deprecated command should still run");
    expect_contains(result.err, "Deprecated command alias `ship`", "deprecated command alias warning");
    expect_contains(result.err, "Deprecated command `legacy deploy`", "deprecated command warning");
    expect_contains(result.err, "Deprecated option `--token`", "deprecated option warning");
    expect_contains(result.err, "Deprecated option alias `-t`", "deprecated option alias warning");

    const auto help_result = run_cli(cli, {"deploy", "--help"});
    expect_equal(help_result.exit_code, 0, "deprecated help exit code");
    expect_contains(help_result.out, "Deprecated: Use `release deploy` instead.", "help deprecated command");
    expect_contains(help_result.out, "deprecated aliases", "help deprecated aliases");

    const auto schema = cli.schema_json();
    expect_contains(schema, "\"deprecated\": true", "schema deprecated flag");
    expect_contains(schema, "\"deprecated_message\": \"Use `release deploy` instead.\"", "schema deprecated message");
    expect_contains(schema, "\"deprecated_aliases\": {\"ship\": \"Use `deploy` instead.\"}", "schema deprecated alias");

    const auto markdown = cli.markdown();
    expect_contains(markdown, "# legacy", "markdown root heading");
    expect_contains(markdown, "## legacy deploy", "markdown child heading");
    expect_contains(markdown, "**Deprecated**", "markdown deprecated bullet");
    expect_contains(markdown, "Deprecated aliases", "markdown deprecated aliases");
}

void test_platform_helpers_report_current_target_consistently() {
    constexpr auto info = clix::platform::current_info();

    expect(info.kind != clix::platform::Kind::Unknown, "platform kind should be detected");
    expect(!clix::platform::name().empty(), "platform name should not be empty");
    expect(!clix::platform::family_name(info.family).empty(), "platform family name should not be empty");
    expect_equal(info.kind, clix::platform::current(), "platform current kind");
    expect_equal(info.family, clix::platform::family(), "platform family");
    expect_equal(info.is_apple, clix::platform::is_apple(), "platform apple helper");
    expect_equal(info.is_bsd, clix::platform::is_bsd(), "platform bsd helper");
    expect_equal(info.is_mobile, clix::platform::is_mobile(), "platform mobile helper");
    expect_equal(info.is_posix, clix::platform::is_posix(), "platform posix helper");
    expect_equal(info.is_desktop, clix::platform::is_desktop(), "platform desktop helper");
    expect(info.has_filesystem, "supported targets should have filesystem support");
    expect(info.has_process_environment, "supported targets should expose process environments");

#if defined(_WIN32)
    expect_equal(info.kind, clix::platform::Kind::Windows, "windows platform kind");
    expect_equal(info.family, clix::platform::Family::Windows, "windows platform family");
    expect(!info.is_posix, "windows should not report posix");
#elif defined(__ANDROID__)
    expect_equal(info.kind, clix::platform::Kind::Android, "android platform kind");
    expect_equal(info.family, clix::platform::Family::Android, "android platform family");
    expect(info.is_mobile, "android should report mobile");
    expect(info.is_posix, "android should report posix");
#elif defined(__APPLE__)
    expect_equal(info.family, clix::platform::Family::Apple, "apple platform family");
    if (info.is_mobile) {
        expect_equal(info.kind, clix::platform::Kind::Ios, "ios platform kind");
    } else {
        expect_equal(info.kind, clix::platform::Kind::Macos, "macos platform kind");
    }
    expect(info.is_posix, "apple targets should report posix");
#elif defined(__linux__)
    expect_equal(info.kind, clix::platform::Kind::Linux, "linux platform kind");
    expect_equal(info.family, clix::platform::Family::Linux, "linux platform family");
    expect(info.is_posix, "linux should report posix");
#elif defined(__FreeBSD__)
    expect_equal(info.kind, clix::platform::Kind::Freebsd, "freebsd platform kind");
    expect_equal(info.family, clix::platform::Family::Bsd, "freebsd platform family");
    expect(info.is_bsd, "freebsd should report bsd");
#elif defined(__OpenBSD__)
    expect_equal(info.kind, clix::platform::Kind::Openbsd, "openbsd platform kind");
    expect_equal(info.family, clix::platform::Family::Bsd, "openbsd platform family");
    expect(info.is_bsd, "openbsd should report bsd");
#elif defined(__NetBSD__)
    expect_equal(info.kind, clix::platform::Kind::Netbsd, "netbsd platform kind");
    expect_equal(info.family, clix::platform::Family::Bsd, "netbsd platform family");
    expect(info.is_bsd, "netbsd should report bsd");
#elif defined(__DragonFly__)
    expect_equal(info.kind, clix::platform::Kind::Dragonflybsd, "dragonflybsd platform kind");
    expect_equal(info.family, clix::platform::Family::Bsd, "dragonflybsd platform family");
    expect(info.is_bsd, "dragonflybsd should report bsd");
#endif
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"help_and_version_exit_success", test_help_and_version_exit_success},
        {"nested_subcommand_invocation_and_defaults", test_nested_subcommand_invocation_and_defaults},
        {"validators_reject_invalid_numeric_values", test_validators_reject_invalid_numeric_values},
        {"existing_path_validator_accepts_and_rejects", test_existing_path_validator_accepts_and_rejects},
        {"config_file_supplies_missing_values_and_merges_sections", test_config_file_supplies_missing_values_and_merges_sections},
        {"command_line_values_override_config_values", test_command_line_values_override_config_values},
        {"strict_config_rejects_unknown_keys", test_strict_config_rejects_unknown_keys},
        {"root_completion_includes_commands_and_builtin_options", test_root_completion_includes_commands_and_builtin_options},
        {"nested_subcommand_and_option_value_completion", test_nested_subcommand_and_option_value_completion},
        {"hidden_completion_backend_writes_lines", test_hidden_completion_backend_writes_lines},
        {"completion_script_generation_supports_all_shells", test_completion_script_generation_supports_all_shells},
        {"option_groups_appear_in_help_output", test_option_groups_appear_in_help_output},
        {"path_completion_suggests_filesystem_entries", test_path_completion_suggests_filesystem_entries},
        {"global_options_before_subcommand_are_supported", test_global_options_before_subcommand_are_supported},
        {"config_values_have_higher_precedence_than_environment", test_config_values_have_higher_precedence_than_environment},
        {"config_path_can_come_from_environment_or_default_filename", test_config_path_can_come_from_environment_or_default_filename},
        {"custom_environment_reader_supplies_declared_values", test_custom_environment_reader_supplies_declared_values},
        {"process_environment_is_used_even_with_custom_reader", test_process_environment_is_used_even_with_custom_reader},
        {"unsupported_config_extensions_are_rejected", test_unsupported_config_extensions_are_rejected},
        {"config_extension_order_is_respected_for_extensionless_paths", test_config_extension_order_is_respected_for_extensionless_paths},
        {"end_of_flags_marker_requires_following_values", test_end_of_flags_marker_requires_following_values},
        {"short_option_groups_support_inline_value_suffixes", test_short_option_groups_support_inline_value_suffixes},
        {"requires_excludes_and_exclusive_groups_are_enforced", test_requires_excludes_and_exclusive_groups_are_enforced},
        {"passthrough_collects_unknown_options_and_extra_arguments", test_passthrough_collects_unknown_options_and_extra_arguments},
        {"hidden_options_are_runtime_only", test_hidden_options_are_runtime_only},
        {"child_commands_override_parent_option_definitions", test_child_commands_override_parent_option_definitions},
        {"router_mounts_modular_command_trees", test_router_mounts_modular_command_trees},
        {"router_reuses_existing_paths_and_rejects_invalid_routes", test_router_reuses_existing_paths_and_rejects_invalid_routes},
        {"url_value_kind_parses_absolute_urls", test_url_value_kind_parses_absolute_urls},
        {"custom_typed_parsers_and_value_sources_are_available_in_invocations",
         test_custom_typed_parsers_and_value_sources_are_available_in_invocations},
        {"validator_composition_helpers_cover_common_constraints",
         test_validator_composition_helpers_cover_common_constraints},
        {"command_bundles_improve_reuse_without_duplicating_schema",
         test_command_bundles_improve_reuse_without_duplicating_schema},
        {"deprecated_commands_options_schema_and_markdown_are_exposed",
         test_deprecated_commands_options_schema_and_markdown_are_exposed},
        {"platform_helpers_report_current_target_consistently",
         test_platform_helpers_report_current_target_consistently},
    };

    std::size_t failures = 0;

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& exception) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
        }
    }

    if (failures > 0) {
        std::cerr << failures << " test(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << tests.size() << " test(s) passed.\n";
    return EXIT_SUCCESS;
}
