#pragma once

#include "Raven/World/World.h"

#include <string>

namespace Raven
{
    // Scene file format, version 1 (line-based text):
    //
    //   raven-scene 1
    //   entity "Cube"
    //     transform px py pz rx ry rz rw sx sy sz
    //     mesh "builtin:cube"
    //   end
    //
    // Names are double-quoted; \" \\ and \n are escaped.
    // Unknown component lines inside an entity are skipped with a warning.

    std::string serializeScene(const World& world);

    // Parses the whole text before touching the world. On error nothing is
    // added and false is returned. On success the scene's entities are appended.
    bool deserializeScene(World& world, const std::string& text);

    bool saveScene(const World& world, const std::string& path);
    bool loadScene(World& world, const std::string& path);
}
