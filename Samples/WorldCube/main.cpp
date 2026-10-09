// Phase 2 smoke test. Runs as two separate processes:
//   RavenWorldCube write <file>  creates a cube and saves the scene
//   RavenWorldCube read  <file>  reopens the scene and verifies the cube

#include "Raven/Core/Application.h"
#include "Raven/Core/Logger.h"
#include "Raven/World/Components.h"
#include "Raven/World/Scene.h"
#include "Raven/World/WorldModule.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
    int usage()
    {
        std::fprintf(stderr, "usage: RavenWorldCube write|read <scene-file>\n");
        return 2;
    }

    int writeScene(Raven::World& world, const std::string& path)
    {
        Raven::Entity cube = world.createEntity("Cube");

        Raven::TransformComponent transform;
        transform.position = {0.0f, 1.0f, 0.0f};
        world.add<Raven::TransformComponent>(cube, transform);
        world.add<Raven::MeshComponent>(cube, "builtin:cube");

        return Raven::saveScene(world, path) ? 0 : 1;
    }

    int readScene(Raven::World& world, const std::string& path)
    {
        if (!Raven::loadScene(world, path))
        {
            return 1;
        }

        int matches = 0;
        for (Raven::Entity entity : world.entities())
        {
            const auto* mesh = world.get<Raven::MeshComponent>(entity);
            const auto* transform = world.get<Raven::TransformComponent>(entity);
            if (world.name(entity) == "Cube" && mesh && transform &&
                mesh->mesh == "builtin:cube" &&
                std::fabs(transform->position.y - 1.0f) < 1e-5f)
            {
                ++matches;
            }
        }

        if (matches != 1)
        {
            std::fprintf(stderr, "expected exactly one matching Cube, found %d\n", matches);
            return 1;
        }
        std::printf("reopened scene: found Cube at y=1 with builtin:cube\n");
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        return usage();
    }
    const std::string mode = argv[1];
    const std::string path = argv[2];
    if (mode != "write" && mode != "read")
    {
        return usage();
    }

    Raven::EngineConfig config;
    config.appName = "WorldCube";
    config.maxFrames = 1;

    Raven::Application app(config);
    auto worldModule = std::make_unique<Raven::WorldModule>();
    Raven::WorldModule* worlds = worldModule.get();
    app.engine().registerModule(std::move(worldModule));

    const int result = (mode == "write") ? writeScene(worlds->world(), path)
                                         : readScene(worlds->world(), path);
    const int engineResult = app.run();
    return result != 0 ? result : engineResult;
}
