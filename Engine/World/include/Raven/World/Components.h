#pragma once

#include "Raven/World/Math.h"

#include <string>

namespace Raven
{
    struct TransformComponent
    {
        Vec3 position;
        Quat rotation;
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    // Refers to a mesh by name, e.g. "builtin:cube". The renderer resolves
    // the name later; the world only stores it.
    struct MeshComponent
    {
        std::string mesh;
    };
}
