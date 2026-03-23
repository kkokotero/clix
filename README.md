# clix

`clix` is a header-only C++ CLI library for building command-line applications with a fluent API, nested subcommands, validators, config files, environment-variable support, and shell completion.

## Highlights

- Header-only package with clean `find_package(clix CONFIG REQUIRED)` integration
- No external runtime dependencies
- C++17 baseline
- Modular public includes through `<clix/...>`
- Fluent builders for arguments and options
- Optional routers for modular command registration in large projects
- Nested subcommands with inherited global options
- Validators, config files, environment variables, and shell completion driven by the same command schema
- Readable error messages with hints for invalid input

## Installation

### CMake package

```cmake
find_package(clix CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE clix::clix)
```

### Includes

Preferred modular includes:

```cpp
#include <clix/cli.hpp>
#include <clix/validators.hpp>
#include <clix/router.hpp>
```

Umbrella include:

```cpp
#include <clix.hpp>
```

## Quick Start

```cpp
#include <cctype>
#include <iostream>
#include <string>

#include <clix/cli.hpp>

int main(int argc, char** argv) {
    clix::CLI cli("hello", "1.0.0");
    cli.description("Small example built with clix.");

    auto& greet = cli.command("greet").description("Print a greeting.");
    greet.arg("name").description("Name to greet.").label("name");
    greet.opt("caps").alias("c").description("Render the greeting in uppercase.");

    greet.action([](const clix::Invocation& invocation) {
        auto message = std::string("Hello, ") + invocation.argument<std::string>("name") + "!";
        if (invocation.option<bool>("caps")) {
            for (auto& character : message) {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
            }
        }

        std::cout << message << '\n';
    });

    return cli.run(argc, argv);
}
```

## Fluent API

The builder API is intentionally small and predictable:

```cpp
create.arg("name")
    .description("Project name.")
    .label("name")
    .env("WORKSPACE_NAME")
    .validate(clix::validators::non_empty_string());

create.opt("jobs", clix::ValueKind::number)
    .alias("j")
    .description("Parallel jobs.")
    .label("count")
    .default_value(clix::CliValue(4.0))
    .validate(clix::validators::number_range(1.0, 32.0));
```

Available builder features include:

- `description(...)`
- `label(...)`
- `optional()` and `required()`
- `default_value(...)`
- `env(...)`
- `choices(...)`
- `complete(...)`
- `validate(...)`
- `alias(...)`
- `group(...)`
- `requires(...)`
- `excludes(...)`
- `exclusive_group(...)`
- `hidden()`

## Routers

`clix::Router` lets large applications register commands in modules and mount them under prefixes.

```cpp
clix::Router app_router;
app_router.use("project", make_project_router());
app_router.use("release", make_release_router());
app_router.mount(cli);
```

This keeps command registration modular while preserving the same `Command` builder API inside each module.

## Nested Subcommands

Commands can be nested as deeply as needed:

```cpp
auto& project = cli.command("project").description("Project lifecycle commands.");
auto& create = project.command("create").description("Create a new project.");
auto& inspect = project.command("inspect").description("Inspect an existing project.");
```

Global options declared on parent commands are visible from child commands, even when the user places them before the subcommand:

```bash
workspace --verbose project create app
```

## Validators

Built-in validators include:

- `clix::validators::non_empty_string()`
- `clix::validators::number_range(min, max)`
- `clix::validators::positive_number()`
- `clix::validators::existing_path()`

Custom validators are regular callables that return `std::optional<std::string>`.

## Option Relationships

`clix` supports a few high-value option relationships without forcing a large DSL:

```cpp
command.opt("push").requires("token");
command.opt("dry-run").excludes("push");
command.opt("json").exclusive_group("output");
command.opt("yaml").exclusive_group("output");
```

This covers:

- required companion options
- conflicting options
- mutually exclusive sets

## Config Files

Enable config file loading once on the root CLI:

```cpp
cli.enable_config_files();
```

Supported syntax:

```ini
language = ts
format = yaml

[project.create]
name = from-config
jobs = 4
region = us-east-1
```

Precedence is:

1. Command line
2. Environment variables
3. Config file
4. Default values

Strict mode is enabled by default, so unknown config keys fail fast.

## Environment Variables

Arguments and options can read from one or more environment variables:

```cpp
command.arg("name").env("APP_NAME");
command.opt("language", clix::ValueKind::choice).env("APP_LANGUAGE");
```

This is useful when you want non-interactive defaults without giving up the command-line UX.

## Passthrough

For wrapper-style commands, `clix` can preserve unknown options and extra arguments:

```cpp
auto& run = cli.command("run").allow_passthrough();
```

The action can access them through:

```cpp
invocation.passthrough_tokens()
```

## Shell Completion

`clix` can generate completion scripts for:

- Bash
- Zsh
- Fish
- PowerShell

Enable it once:

```cpp
cli.enable_completion();
```

Generate the script at runtime:

```bash
./your-cli --generate-completion bash
./your-cli --generate-completion zsh
./your-cli --generate-completion fish
./your-cli --generate-completion powershell
```

Completion can suggest:

- command names
- nested subcommands
- option names
- choice values
- boolean values
- filesystem paths
- static custom values
- dynamic provider-backed values

## Error Handling

The parser now fails fast on malformed invocations such as:

- missing option values
- `app --`
- unknown commands
- conflicting options
- missing related options

Errors include context and, when possible, suggestions.

## Examples

- [examples/hello/](examples/hello/) - simple greeting app
- [examples/calculator/](examples/calculator/) - small arithmetic CLI
- [examples/file_editor/](examples/file_editor/) - tiny text file editor
- [examples/random_number/](examples/random_number/) - random integer generator
- [examples/README.md](examples/README.md) - overview and build notes

## Documentation

- [docs/README.md](docs/README.md)
- [docs/architecture.md](docs/architecture.md)
- [docs/router.md](docs/router.md)
- [docs/project-layout.md](docs/project-layout.md)
- [docs/benchmarks.md](docs/benchmarks.md)

Build them with:

```bash
cmake --preset default -DCLIX_BUILD_EXAMPLES=ON
cmake --build build -j
```

## Benchmarks

Build and run the benchmark harness with:

```bash
cmake --preset default -DCLIX_BUILD_BENCHMARKS=ON
cmake --build build -j
./build/benchmarks/clix_benchmarks
```

## Tests

The test suite covers:

- nested subcommands
- defaults and required values
- validators
- config parsing and precedence
- environment variables
- global options before subcommands
- short option groups with inline values
- passthrough mode
- hidden options
- mutually exclusive options
- requires and excludes
- child overrides over parent options
- completion suggestions and script generation

Run it with:

```bash
cmake --preset default -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```
