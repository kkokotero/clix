# Platform Support

`CLIX` is intentionally conservative about platform coupling. The core library stays header-only, uses the C++17 standard library, and avoids OS-specific runtime dependencies in the parser, schema model, config loader, validators, and Markdown export pipeline.

## Target Platforms

`CLIX` is designed for:

- Linux
- macOS
- Windows
- iOS and iOS Simulator
- Android through the NDK
- FreeBSD
- OpenBSD
- NetBSD
- DragonFly BSD

## Validation Levels

- Native CI:
  - Linux
  - macOS
  - Windows
- Cross-toolchain smoke coverage:
  - iOS Simulator header compilation
  - Android NDK header compilation
- Source-compatible targets:
  - BSD toolchains are detected and supported by the public platform API, but this repository does not yet have hosted BSD CI runners

That split matters because `CLIX` is mostly a header-only schema and parsing library. For a project like this, compile-time validation across toolchains is often the key portability check, while runtime shell integration still depends on the shells available in the target environment.

## Public Platform Helpers

Include the public helpers with:

```cpp
#include <clix/platform.hpp>
```

The header exposes:

- `clix::platform::Kind`
- `clix::platform::Family`
- `clix::platform::current()`
- `clix::platform::family()`
- `clix::platform::name()`
- `clix::platform::family_name(...)`
- `clix::platform::is_apple()`
- `clix::platform::is_bsd()`
- `clix::platform::is_mobile()`
- `clix::platform::is_posix()`
- `clix::platform::is_desktop()`
- `clix::platform::current_info()`

These helpers are meant to keep platform-aware command registration readable and localized.

## Example

```cpp
#include <clix/cli.hpp>
#include <clix/platform.hpp>

int main(int argc, char** argv) {
    clix::CLI cli("portable-demo", "1.0.0");

    if (clix::platform::is_mobile()) {
        cli.command("sync").description("Sync cached data on mobile targets.");
    } else {
        cli.command("shell").description("Open a richer desktop workflow.");
    }

    return cli.run(argc, argv);
}
```

## Feature Notes

- Completion script generation is portable, but installing or sourcing those scripts still depends on which shell exists on the host.
- `ValueKind::path`, config-file discovery, and filesystem completion rely on a toolchain with `std::filesystem`.
- Environment-variable support uses the process environment, so it behaves naturally on desktop and server targets. Mobile targets may still compile the library cleanly, but the surrounding application environment determines how useful environment-driven configuration is in practice.
