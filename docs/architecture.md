# Architecture

`clix` is built around a simple idea: define a command schema once and reuse it for the full command-line workflow.

The same command model drives:

- parsing
- validation
- help output
- JSON/TOML config file loading
- environment-variable resolution
- deprecation warnings
- schema export
- Markdown export
- shell completion

## Core Types

- `clix::CLI`: root command entry point
- `clix::Command`: reusable command builder for root and nested commands
- `clix::Invocation`: immutable runtime view delivered to handlers
- `clix::Router`: optional composition layer for modular command registration

## Design Principles

- Keep the public API small and readable.
- Prefer composition over inheritance.
- Let metadata power multiple runtime features.
- Avoid external dependencies.
- Stay friendly to older C++ standards where practical.

## Command Schema

Commands are trees. Each node can define:

- subcommands
- positional arguments
- options
- option groups
- custom typed parsers
- validators
- config-backed values
- deprecation metadata
- completion metadata
- one action handler

This makes `clix` a schema-driven CLI runtime rather than a plain option parser.

## Parsing Flow

At runtime `clix` resolves values in this order:

1. command line
2. config file
3. environment variables
4. default values

Config files themselves are discovered in this order:

1. explicit `--config`
2. configured config-path environment variables
3. configured default filenames

If a configured path is extensionless, `clix` probes the configured allowed extensions in order.

After values are resolved, validators and option relationships are enforced.

Resolved values also keep source metadata so handlers can inspect whether a value came from the command line, config, environment, or defaults.

## When To Use Routers

Use the raw builder API when:

- your CLI is small
- all commands live near one another
- you want the lowest possible abstraction

Use `clix::Router` when:

- your command tree is spread across multiple modules
- different teams own different command branches
- you want to mount reusable command bundles under a shared prefix
