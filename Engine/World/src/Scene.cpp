#include "Raven/World/Scene.h"

#include "Raven/Core/Logger.h"
#include "Raven/World/Components.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <vector>

namespace Raven
{
    namespace
    {
        constexpr int kSceneVersion = 1;

        // 9 significant digits round-trip any float exactly.
        constexpr int kFloatDigits = 9;

        std::string quote(const std::string& text)
        {
            std::string out = "\"";
            for (char c : text)
            {
                switch (c)
                {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    default:   out += c;      break;
                }
            }
            out += '"';
            return out;
        }

        // Reads a quoted string from the next token in the stream.
        bool readQuoted(std::istringstream& in, std::string& out)
        {
            in >> std::ws;
            if (in.get() != '"')
            {
                return false;
            }

            out.clear();
            for (;;)
            {
                const int c = in.get();
                if (c == EOF)
                {
                    return false;
                }
                if (c == '"')
                {
                    return true;
                }
                if (c == '\\')
                {
                    const int escaped = in.get();
                    if (escaped == EOF)
                    {
                        return false;
                    }
                    out += (escaped == 'n') ? '\n' : static_cast<char>(escaped);
                }
                else
                {
                    out += static_cast<char>(c);
                }
            }
        }

        // An entity read from the file, held until the whole file has parsed.
        struct EntityRecord
        {
            std::string name;
            std::optional<TransformComponent> transform;
            std::optional<MeshComponent> mesh;
        };
    }

    std::string serializeScene(const World& world)
    {
        std::ostringstream out;
        out << std::setprecision(kFloatDigits);
        out << "raven-scene " << kSceneVersion << "\n";

        for (Entity entity : world.entities())
        {
            out << "entity " << quote(world.name(entity)) << "\n";

            if (const auto* t = world.get<TransformComponent>(entity))
            {
                out << "  transform"
                    << ' ' << t->position.x << ' ' << t->position.y << ' ' << t->position.z
                    << ' ' << t->rotation.x << ' ' << t->rotation.y << ' ' << t->rotation.z
                    << ' ' << t->rotation.w
                    << ' ' << t->scale.x << ' ' << t->scale.y << ' ' << t->scale.z
                    << "\n";
            }
            if (const auto* m = world.get<MeshComponent>(entity))
            {
                out << "  mesh " << quote(m->mesh) << "\n";
            }

            out << "end\n";
        }
        return out.str();
    }

    bool deserializeScene(World& world, const std::string& text)
    {
        std::vector<EntityRecord> records;
        std::istringstream lines(text);
        std::string line;
        int lineNo = 0;
        bool sawHeader = false;
        bool inEntity = false;
        EntityRecord current;

        auto fail = [&lineNo](const std::string& reason) {
            Log::error("scene line " + std::to_string(lineNo) + ": " + reason);
            return false;
        };

        while (std::getline(lines, line))
        {
            ++lineNo;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            std::istringstream tokens(line);
            std::string keyword;
            if (!(tokens >> keyword) || keyword[0] == '#')
            {
                continue; // blank line or comment
            }

            if (!sawHeader)
            {
                int version = 0;
                if (keyword != "raven-scene" || !(tokens >> version))
                {
                    return fail("expected 'raven-scene <version>' header");
                }
                if (version != kSceneVersion)
                {
                    return fail("unsupported scene version " + std::to_string(version));
                }
                sawHeader = true;
                continue;
            }

            if (keyword == "entity")
            {
                if (inEntity)
                {
                    return fail("'entity' inside an entity block");
                }
                current = EntityRecord{};
                if (!readQuoted(tokens, current.name))
                {
                    return fail("entity name must be a quoted string");
                }
                inEntity = true;
            }
            else if (keyword == "end")
            {
                if (!inEntity)
                {
                    return fail("'end' without 'entity'");
                }
                records.push_back(current);
                inEntity = false;
            }
            else if (keyword == "transform")
            {
                if (!inEntity)
                {
                    return fail("'transform' outside an entity block");
                }
                TransformComponent t;
                if (!(tokens >> t.position.x >> t.position.y >> t.position.z
                              >> t.rotation.x >> t.rotation.y >> t.rotation.z >> t.rotation.w
                              >> t.scale.x >> t.scale.y >> t.scale.z))
                {
                    return fail("malformed transform (expected 10 numbers)");
                }
                current.transform = t;
            }
            else if (keyword == "mesh")
            {
                if (!inEntity)
                {
                    return fail("'mesh' outside an entity block");
                }
                MeshComponent m;
                if (!readQuoted(tokens, m.mesh))
                {
                    return fail("mesh name must be a quoted string");
                }
                current.mesh = m;
            }
            else
            {
                if (!inEntity)
                {
                    return fail("unexpected keyword '" + keyword + "'");
                }
                Log::warning("scene line " + std::to_string(lineNo) +
                             ": unknown component '" + keyword + "' skipped");
            }
        }

        if (!sawHeader)
        {
            Log::error("scene is empty or missing the 'raven-scene' header");
            return false;
        }
        if (inEntity)
        {
            return fail("unterminated entity block at end of file");
        }

        // Every line parsed. Only now modify the world.
        for (const EntityRecord& record : records)
        {
            Entity entity = world.createEntity(record.name);
            if (record.transform)
            {
                world.add<TransformComponent>(entity, *record.transform);
            }
            if (record.mesh)
            {
                world.add<MeshComponent>(entity, *record.mesh);
            }
        }
        return true;
    }

    bool saveScene(const World& world, const std::string& path)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            Log::error("cannot open '" + path + "' for writing");
            return false;
        }
        file << serializeScene(world);
        if (!file)
        {
            Log::error("failed while writing '" + path + "'");
            return false;
        }
        Log::info("saved scene '" + path + "' (" + std::to_string(world.entityCount()) +
                  " entities)");
        return true;
    }

    bool loadScene(World& world, const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            Log::error("cannot open scene '" + path + "'");
            return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();

        if (!deserializeScene(world, buffer.str()))
        {
            return false;
        }
        Log::info("loaded scene '" + path + "'");
        return true;
    }
}
