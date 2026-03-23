# Examples

`clix` now ships with four small examples focused on common beginner-friendly tasks.

## Included Projects

- `hello/`
  - Print a greeting with a few friendly options.
  - Good for learning positional arguments, booleans, and defaults.
- `calculator/`
  - Run small arithmetic commands.
  - Good for learning subcommands, shared options, and validators.
- `file_editor/`
  - Read, write, append, and replace text in a file.
  - Good for learning path values and required options.
- `random_number/`
  - Generate random integers with optional seeding.
  - Good for learning number options, choices, and custom validation.

## Build

```bash
cmake --preset default -DCLIX_BUILD_EXAMPLES=ON
cmake --build build -j
```

Generated binaries:

- `build/examples/hello`
- `build/examples/calculator`
- `build/examples/file-editor`
- `build/examples/random-number`
