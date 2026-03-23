# Router Guide

`clix::Router` is a small composition layer for large command-line applications.

It helps you register command trees from multiple modules without forcing a framework-specific application layout.

## Why Use It

Routers are useful when:

- command trees are split across several files
- each domain owns its own subcommands
- you want to mount a whole module under a prefix like `project` or `release`

## Main Operations

- `root(...)`: configure the current mounted command
- `command("path/to/command", ...)`: configure a nested command
- `use(router)`: merge another router at the current level
- `use("prefix", router)`: mount another router under a prefix
- `mount(cli_or_command)`: apply the router to a `CLI` or `Command`

## Example

```cpp
#include <clix/router.hpp>

clix::Router make_project_router() {
    clix::Router router;

    router.root([](clix::Command& project) {
        project.description("Project lifecycle commands.");
    });

    router.command("create", [](clix::Command& create) {
        create.description("Create a project.");
        create.arg("name").description("Project name.");
    });

    router.command("inspect", [](clix::Command& inspect) {
        inspect.description("Inspect a project.");
        inspect.arg("manifest", clix::ValueKind::path).description("Manifest path.");
    });

    return router;
}

int main(int argc, char** argv) {
    clix::CLI cli("workspace", "0.2.0");

    clix::Router app_router;
    app_router.use("project", make_project_router());
    app_router.mount(cli);

    return cli.run(argc, argv);
}
```

## Recommended Module Pattern

Each domain exports one function that returns a router:

```text
src/
  commands/
    project/
      router.hpp
      router.cpp
    release/
      router.hpp
      router.cpp
```

Then the application entry point mounts them:

```cpp
app_router.use("project", make_project_router());
app_router.use("release", make_release_router());
```

## Practical Advice

- Keep business logic outside the router lambdas.
- Use routers to describe command structure, not application state.
- Let services own the real work.
- Treat route paths as canonical command names, not aliases.
- Prefer small routers per domain over one large global file.
