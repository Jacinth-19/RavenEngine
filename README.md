# RavenEngine

A C++17 game engine. This is an early, Phase 1 skeleton.

## Layout

```
Engine/Core/      RavenCore static library: Logger, IModule, Engine, Application
Samples/HelloCore Headless sample app that runs one module for 3 frames
Tests/Core        Dependency-free unit tests for RavenCore (run via CTest)
```

Dependency direction: higher layers depend on lower ones, never the reverse.
Modules are the extension point. Renderer, physics, and audio will each be a
module registered with the `Engine`.

## Core concepts

- `IModule`: `initialize()` runs in registration order, `update(dt)` runs every
  frame, and `shutdown()` runs in reverse order.
- `Engine`: owns modules and drives their lifecycle. If a module fails to
  initialize, only the modules that already initialized are shut down.
- `Application`: owns an `Engine` and runs the main loop.

## Build and test

Requires CMake 3.20+ and a C++17 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/Samples/HelloCore/RavenHelloCore
```

## Provenance

Raven is written from scratch. The project studies the architecture of other
open-source engines (e.g. Thunder Engine) as reference only and does not copy
their source code. If that policy changes, third-party code must be added with
its license and attribution preserved.

## License

Not yet decided. No LICENSE file is present.
