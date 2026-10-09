# RavenEngine

A C++17 game engine. This is an early, Phase 1 skeleton.

## Layout

```
Engine/Core/      RavenCore static library: Logger, IModule, Engine, Application
Engine/World/     RavenWorld static library: World, Entity, components, scene I/O
Engine/Render/    RavenRender (backend-neutral RHI interface, header-only)
Engine/Render/Vulkan/  RavenVulkan: the Vulkan implementation of the RHI
Engine/Render/Shaders/ RavenShaders: GLSL compiled to SPIR-V at build time
tools/shaders/    glslang-based GLSL->SPIR-V compiler (Node.js, build-time only)
Samples/HelloCore Headless sample app that runs one module for 3 frames
Samples/WorldCube Saves a cube in one process and finds it in a fresh process
Samples/RenderTriangle  Renders a triangle offscreen with Vulkan and writes a PNG
Tests/Core        Dependency-free unit tests for RavenCore (run via CTest)
Tests/World       Dependency-free unit tests for RavenWorld (run via CTest)
Tests/Render      Vulkan render-device tests (skipped if no Vulkan device)
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

## World (Phase 2)

- `Entity`: generational handle. Destroyed slots are reused, and stale handles
  are rejected by `World::isAlive()`.
- Components are stored per type. Use `add<T>`, `get<T>`, `has<T>`, `remove<T>`,
  and `entitiesWith<T>`. Destroying an entity removes all of its components.
- `WorldModule` is an engine module that owns a `World`.
- Built-in components: `TransformComponent` (position, quaternion rotation,
  scale) and `MeshComponent` (mesh name such as `builtin:cube`).

### Scene file (version 1, text)

```
raven-scene 1
entity "Cube"
  transform 0 1 0 0 0 0 1 1 1 1
  mesh "builtin:cube"
end
```

Loading is atomic: a file with any error adds no entities. Unknown component
lines are skipped with a warning. Serialize/deserialize are fully round-trip.

Adding a component type means adding it to `Scene.cpp`. A generic component
registry is deferred until there are more types.

## Renderer (Phase 3)

`IRenderDevice` (in `RenderDevice.h`) is the backend-neutral interface. It has
handle-based buffers, shaders, pipelines, and offscreen render targets, plus a
frame model: `beginFrame` -> `beginPass` -> bind/draw -> `endPass` -> `endFrame`,
then `readPixels`. Only Vulkan is implemented so far. The render target is
offscreen; swapchain/window presentation is not built yet.

### Renderer prerequisites

- **Node.js** (used at build time to compile GLSL with glslang). One-time setup:
  `npm --prefix tools/shaders ci`
- **Vulkan loader and headers**. CMake uses `find_package(Vulkan)`. Pass
  `-DVulkan_INCLUDE_DIR=...` and `-DVulkan_LIBRARY=...` if they are not in a
  standard location.
- A **Vulkan driver (ICD)**. On a machine with no GPU, a software driver such as
  SwiftShader works. Point the loader at it with
  `VK_ICD_FILENAMES=/path/to/vk_swiftshader_icd.json`.

Build with `-DRAVEN_BUILD_RENDER=OFF` to skip the renderer entirely. Render
tests exit 77 (CTest "skipped") when no Vulkan device is available.

## Build and test

Requires CMake 3.20+ and a C++17 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/Samples/HelloCore/RavenHelloCore
./build/Samples/RenderTriangle/RavenRenderTriangle triangle.png   # needs a Vulkan device
```

## Provenance

Raven is written from scratch. The project studies the architecture of other
open-source engines (e.g. Thunder Engine) as reference only and does not copy
their source code. If that policy changes, third-party code must be added with
its license and attribution preserved.

## License

Not yet decided. No LICENSE file is present.
