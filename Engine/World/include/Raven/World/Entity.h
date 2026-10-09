#pragma once

#include <cstdint>

namespace Raven
{
    // Generational handle. A destroyed entity's slot can be reused, but the
    // generation changes, so stale handles are rejected by World::isAlive().
    struct Entity
    {
        std::uint32_t index = 0;
        std::uint32_t generation = 0; // 0 is the null entity

        bool isNull() const { return generation == 0; }

        friend bool operator==(const Entity& a, const Entity& b)
        {
            return a.index == b.index && a.generation == b.generation;
        }
        friend bool operator!=(const Entity& a, const Entity& b) { return !(a == b); }
    };
}
