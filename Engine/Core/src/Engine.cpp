#include "Raven/Core/Engine.h"
#include "Raven/Core/Logger.h"

#include <utility>

namespace Raven
{
    Engine::Engine(EngineConfig config)
        : m_config(std::move(config))
    {
    }

    Engine::~Engine()
    {
        shutdown();
    }

    bool Engine::registerModule(std::unique_ptr<IModule> module)
    {
        if (!module)
        {
            Log::error("registerModule: null module");
            return false;
        }
        if (m_initialized)
        {
            Log::error("registerModule: '" + module->name() + "' registered after initialize()");
            return false;
        }
        for (const auto& existing : m_modules)
        {
            if (existing->name() == module->name())
            {
                Log::error("registerModule: duplicate module name '" + module->name() + "'");
                return false;
            }
        }

        Log::debug("registerModule: " + module->name());
        m_modules.push_back(std::move(module));
        return true;
    }

    bool Engine::initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        Log::info("Engine '" + m_config.appName + "' initializing " +
                  std::to_string(m_modules.size()) + " module(s)");

        m_initializedCount = 0;
        for (auto& module : m_modules)
        {
            if (!module->initialize())
            {
                Log::error("Module '" + module->name() + "' failed to initialize; rolling back");
                shutdown();
                return false;
            }
            ++m_initializedCount;
        }

        m_initialized = true;
        return true;
    }

    void Engine::update(float deltaTime)
    {
        if (!m_initialized)
        {
            return;
        }

        for (auto& module : m_modules)
        {
            module->update(deltaTime);
        }

        ++m_frameCount;

        if (m_config.maxFrames != 0 && m_frameCount >= m_config.maxFrames)
        {
            m_exitRequested = true;
        }
    }

    void Engine::shutdown()
    {
        // Only modules that were successfully initialized get shut down,
        // in reverse order.
        while (m_initializedCount > 0)
        {
            --m_initializedCount;
            m_modules[m_initializedCount]->shutdown();
        }

        if (m_initialized)
        {
            Log::info("Engine '" + m_config.appName + "' shut down after " +
                      std::to_string(m_frameCount) + " frame(s)");
        }
        m_initialized = false;
    }
}
