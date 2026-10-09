#pragma once

#include "Raven/Core/Engine.h"

namespace Raven
{
    // Owns an Engine and runs the main loop with a fixed-timestep-free,
    // measured delta time. Platform windowing will hook in here later.
    class Application
    {
    public:
        explicit Application(EngineConfig config = {});

        Engine& engine() { return m_engine; }
        const Engine& engine() const { return m_engine; }

        // Initializes the engine, loops until exit is requested or maxFrames is
        // reached, then shuts down. Returns process exit code (0 = success).
        int run();

    private:
        Engine m_engine;
    };
}
