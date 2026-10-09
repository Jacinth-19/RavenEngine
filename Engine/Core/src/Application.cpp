#include "Raven/Core/Application.h"
#include "Raven/Core/Logger.h"

#include <chrono>
#include <utility>

namespace Raven
{
    Application::Application(EngineConfig config)
        : m_engine(std::move(config))
    {
    }

    int Application::run()
    {
        if (!m_engine.initialize())
        {
            Log::error("Application failed to initialize");
            return 1;
        }

        using Clock = std::chrono::steady_clock;
        auto previous = Clock::now();

        while (!m_engine.exitRequested())
        {
            const auto now = Clock::now();
            const std::chrono::duration<float> delta = now - previous;
            previous = now;

            m_engine.update(delta.count());
        }

        m_engine.shutdown();
        return 0;
    }
}
