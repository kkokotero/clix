# Project Layout

This is a recommended repository layout for applications built with `clix`.

## Suggested Structure

```text
src/
  main.cpp
  app/
    build_cli.hpp
    build_cli.cpp
  commands/
    project/
      router.hpp
      router.cpp
      handlers/
        create.hpp
        create.cpp
        inspect.hpp
        inspect.cpp
    release/
      router.hpp
      router.cpp
  services/
    project_service.hpp
    project_service.cpp
    release_service.hpp
    release_service.cpp
  validators/
    project_validators.hpp
    project_validators.cpp
  completion/
    regions.hpp
    regions.cpp
  config/
    settings.hpp
    settings.cpp
```

## Responsibilities

- `main.cpp`
  - start the program
  - create the root `clix::CLI`
  - call the app builder
  - return `cli.run(...)`

- `app/build_cli.*`
  - enable completion and config files
  - register global options
  - mount feature routers

- `commands/*/router.*`
  - define command structure for one domain
  - bind handlers
  - keep only CLI wiring here

- `commands/*/handlers/*`
  - translate `Invocation` into service inputs
  - perform small orchestration tasks

- `services/*`
  - hold business logic
  - stay independent from `clix` where possible

- `validators/*`
  - keep custom validators reusable

- `completion/*`
  - keep dynamic completion providers reusable

## Good Rule Of Thumb

If a lambda inside a command action starts looking like business logic, move that work into a service.

`clix` should define the command interface, not own your application model.
