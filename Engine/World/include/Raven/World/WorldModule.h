#pragma once

#include "Raven/Core/Module.h"
#include "Raven/World/World.h"

namespace Raven
{
    // Engine module that owns the World. Other modules reach the world through it.
    class WorldModule final : public IModule
    {
    public:
        std::string name() const override { return "World"; }
        bool initialize() override { return true; }
        void update(float deltaTime) override { (void)deltaTime; }
        void shutdown() override {}

        World& world() { return m_world; }
        const World& world() const { return m_world; }

    private:
        World m_world;
    };
}
