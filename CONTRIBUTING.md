# Contributing to CLIX

Thank you for contributing to CLIX.

The goal of this project is to keep a modern C++ CLI toolkit small, understandable, and reliable. Contributions that improve clarity, correctness, documentation, tests, and developer experience are welcome.

## Ways to Contribute

You can help by:

- reporting bugs
- improving documentation
- adding examples
- expanding tests
- improving diagnostics and error messages
- refining completion, config, or validation behavior
- proposing API improvements that stay aligned with the project's design goals

## Before You Start

For small fixes, feel free to open a pull request directly.

For larger changes, please open an issue first so we can align on scope, API shape, and backward-compatibility impact before implementation starts.

Examples of changes that should usually be discussed first:

- new public API surface
- behavioral changes in parsing or precedence
- new config or completion behavior
- changes that affect supported C++ versions
- anything that adds runtime dependencies

## Local Setup

Configure and build:

```bash
cmake --preset default -DCLIX_BUILD_EXAMPLES=ON -DCLIX_BUILD_BENCHMARKS=ON -DBUILD_TESTING=ON
cmake --build build -j
```

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

Run benchmarks when performance-sensitive code changes:

```bash
./build/benchmarks/clix_benchmarks
```

## Contribution Guidelines

Please keep the following project rules in mind:

- Use English for code comments, docs, issues, and pull requests.
- Keep the public API small and predictable.
- Preserve the header-only and no-runtime-dependencies goals.
- Prefer simple designs over feature-heavy abstractions.
- Avoid breaking changes unless they are clearly justified and documented.
- Add or update tests when changing parsing, validation, config loading, completion, or precedence rules.
- Update documentation when public behavior changes.

## Code Style

When contributing code:

- target C++17 compatibility unless the project explicitly changes its baseline
- prefer readable, explicit code over clever code
- keep helpers small and focused
- avoid introducing hidden global state
- preserve cross-platform behavior where possible
- prefer extending existing patterns before inventing parallel ones

## Pull Request Checklist

Before opening a pull request, please make sure:

- the code builds cleanly
- the relevant tests pass
- new behavior is covered by tests when practical
- docs are updated when needed
- examples are updated when the user-facing API changes
- commit messages are written in English

## Commit Messages

Commit messages should be written in English.

Conventional Commit style is welcome, but not required. Clear, imperative commit messages are preferred.

Good examples:

- `feat: add JSON/TOML config discovery`
- `fix: preserve child option overrides in completion`
- `docs: clarify value precedence`

## Review Expectations

Reviews focus on:

- correctness
- API clarity
- backward compatibility
- documentation quality
- tests
- maintainability

Feedback is meant to improve the project, not to discourage contributors. Questions and follow-up iterations are always okay.

## Need Help?

If you are unsure whether an idea fits the project, open an issue and describe:

- the use case
- the proposed API or behavior
- alternatives you considered
- any compatibility concerns

That usually leads to the fastest path forward.
