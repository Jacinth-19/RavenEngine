// Dependency-free tests for the World, its components, and scene serialization.

#include "Raven/Core/Logger.h"
#include "Raven/World/Components.h"
#include "Raven/World/Scene.h"
#include "Raven/World/World.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    int g_failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

    bool nearly(float a, float b) { return std::fabs(a - b) < 1e-6f; }

    void testEntityLifecycleAndHandleReuse()
    {
        Raven::World world;
        Raven::Entity a = world.createEntity("A");
        Raven::Entity b = world.createEntity("B");

        CHECK(world.isAlive(a));
        CHECK(world.isAlive(b));
        CHECK(world.entityCount() == 2);
        CHECK(world.name(a) == "A");

        CHECK(world.destroyEntity(a));
        CHECK(!world.isAlive(a));
        CHECK(!world.destroyEntity(a));      // double destroy
        CHECK(world.entityCount() == 1);
        CHECK(world.name(a).empty());        // stale handle has no name

        Raven::Entity c = world.createEntity("C"); // reuses a's slot
        CHECK(c.index == a.index);
        CHECK(c.generation != a.generation);
        CHECK(!world.isAlive(a));            // stale handle still rejected
        CHECK(world.isAlive(c));
        CHECK(world.name(c) == "C");
        CHECK(world.isAlive(b));
    }

    void testNullEntityIsNeverAlive()
    {
        Raven::World world;
        Raven::Entity none;
        CHECK(none.isNull());
        CHECK(!world.isAlive(none));
        CHECK(!world.destroyEntity(none));
        CHECK(world.add<Raven::MeshComponent>(none, "x") == nullptr);
    }

    void testComponentAddGetRemove()
    {
        Raven::World world;
        Raven::Entity e = world.createEntity("E");

        CHECK(!world.has<Raven::MeshComponent>(e));
        auto* mesh = world.add<Raven::MeshComponent>(e, "builtin:cube");
        CHECK(mesh != nullptr);
        CHECK(world.has<Raven::MeshComponent>(e));
        CHECK(world.get<Raven::MeshComponent>(e)->mesh == "builtin:cube");

        world.add<Raven::MeshComponent>(e, "builtin:sphere"); // replaces
        CHECK(world.get<Raven::MeshComponent>(e)->mesh == "builtin:sphere");

        CHECK(world.remove<Raven::MeshComponent>(e));
        CHECK(!world.has<Raven::MeshComponent>(e));
        CHECK(!world.remove<Raven::MeshComponent>(e));
    }

    void testDestroyRemovesComponents()
    {
        Raven::World world;
        Raven::Entity e = world.createEntity("E");
        world.add<Raven::TransformComponent>(e);
        world.add<Raven::MeshComponent>(e, "builtin:cube");
        world.destroyEntity(e);

        Raven::Entity reused = world.createEntity("Reused"); // same slot
        CHECK(reused.index == e.index);
        CHECK(!world.has<Raven::TransformComponent>(reused));
        CHECK(!world.has<Raven::MeshComponent>(reused));
        CHECK(world.get<Raven::MeshComponent>(e) == nullptr); // stale handle
    }

    void testEntitiesWithAndClear()
    {
        Raven::World world;
        Raven::Entity a = world.createEntity("A");
        Raven::Entity b = world.createEntity("B");
        world.createEntity("C");
        world.add<Raven::MeshComponent>(a, "m");
        world.add<Raven::MeshComponent>(b, "m");

        CHECK(world.entitiesWith<Raven::MeshComponent>().size() == 2);
        CHECK(world.entities().size() == 3);

        world.clear();
        CHECK(world.entityCount() == 0);
        CHECK(!world.isAlive(a));
        CHECK(world.entitiesWith<Raven::MeshComponent>().empty());
    }

    void testSceneRoundTripString()
    {
        Raven::World original;
        Raven::Entity cube = original.createEntity("Crate \"big\" \\ one\nline two");
        Raven::TransformComponent t;
        t.position = {0.1f, -2.5f, 1e-4f};
        t.rotation = {0.0f, 0.70710677f, 0.0f, 0.70710677f};
        t.scale = {3.0f, 3.0f, 3.0f};
        original.add<Raven::TransformComponent>(cube, t);
        original.add<Raven::MeshComponent>(cube, "builtin:cube");
        original.createEntity("Empty");

        const std::string text = serializeScene(original);

        Raven::World loaded;
        CHECK(deserializeScene(loaded, text));
        CHECK(loaded.entityCount() == 2);

        Raven::Entity found;
        for (Raven::Entity e : loaded.entities())
        {
            if (loaded.name(e) == original.name(cube))
            {
                found = e;
            }
        }
        CHECK(!found.isNull());
        const auto* lt = loaded.get<Raven::TransformComponent>(found);
        CHECK(lt != nullptr);
        if (lt)
        {
            CHECK(nearly(lt->position.x, 0.1f));
            CHECK(nearly(lt->position.y, -2.5f));
            CHECK(nearly(lt->position.z, 1e-4f));
            CHECK(nearly(lt->rotation.y, 0.70710677f));
            CHECK(nearly(lt->scale.x, 3.0f));
        }
        CHECK(loaded.get<Raven::MeshComponent>(found) != nullptr);
        CHECK(loaded.get<Raven::MeshComponent>(found)->mesh == "builtin:cube");

        // Serializing the loaded world reproduces the same text.
        CHECK(serializeScene(loaded) == text);
    }

    void testMalformedSceneIsAtomic()
    {
        Raven::World world;
        const std::string text =
            "raven-scene 1\n"
            "entity \"Good\"\n"
            "  mesh \"builtin:cube\"\n"
            "end\n"
            "entity \"Bad\"\n"
            "  transform 1 2 three\n"   // malformed
            "end\n";

        CHECK(!deserializeScene(world, text));
        CHECK(world.entityCount() == 0); // nothing from the good entity leaked in
    }

    void testBadHeadersAndStructureRejected()
    {
        Raven::World world;
        CHECK(!deserializeScene(world, ""));
        CHECK(!deserializeScene(world, "entity \"X\"\nend\n"));
        CHECK(!deserializeScene(world, "raven-scene 99\n"));
        CHECK(!deserializeScene(world, "raven-scene 1\nentity \"Open\"\n"));      // no end
        CHECK(!deserializeScene(world, "raven-scene 1\nend\n"));                 // end w/o entity
        CHECK(!deserializeScene(world, "raven-scene 1\nentity Unquoted\nend\n")); // no quotes
        CHECK(!deserializeScene(world, "raven-scene 1\nentity \"A\"\n  mesh bare\nend\n"));
        CHECK(world.entityCount() == 0);
    }

    void testUnknownComponentIsSkipped()
    {
        Raven::World world;
        const std::string text =
            "raven-scene 1\n"
            "# a comment\n"
            "entity \"Mover\"\n"
            "  velocity 1 2 3\n"
            "  mesh \"builtin:cube\"\n"
            "end\n";
        CHECK(deserializeScene(world, text));
        CHECK(world.entityCount() == 1);
        auto found = world.entitiesWith<Raven::MeshComponent>();
        CHECK(found.size() == 1);
    }

    void testSaveAndLoadFile()
    {
        const std::string path = "raven_world_test.rscene";
        Raven::World saved;
        Raven::Entity e = saved.createEntity("File");
        saved.add<Raven::MeshComponent>(e, "builtin:cube");

        CHECK(Raven::saveScene(saved, path));

        Raven::World loaded;
        CHECK(Raven::loadScene(loaded, path));
        CHECK(loaded.entityCount() == 1);

        CHECK(!Raven::loadScene(loaded, "does_not_exist.rscene"));
        std::remove(path.c_str());
    }
}

int main()
{
    Raven::Log::setLevel(Raven::LogLevel::Off); // keep test output clean

    testEntityLifecycleAndHandleReuse();
    testNullEntityIsNeverAlive();
    testComponentAddGetRemove();
    testDestroyRemovesComponents();
    testEntitiesWithAndClear();
    testSceneRoundTripString();
    testMalformedSceneIsAtomic();
    testBadHeadersAndStructureRejected();
    testUnknownComponentIsSkipped();
    testSaveAndLoadFile();

    if (g_failures == 0)
    {
        std::printf("RavenWorld tests: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "RavenWorld tests: %d check(s) failed\n", g_failures);
    return 1;
}
