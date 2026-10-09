#pragma once

#include "Raven/Core/Module.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Raven
{
    struct EngineConfig
    {
        std::string appName = "Raven";

        // Stop after this many frames. 0 means run until requestExit().
        // Useful for headless runs and tests.
        std::uint64_t maxFrames = 0;
    };

    class Engine
    {
    public:
        explicit Engine(EngineConfig config = {});
        ~Engine();

        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // Modules may only be registered before initialize().
        // Returns false on null module, duplicate name, or if already initialized.
        bool registerModule(std::unique_ptr<IModule> module);

        // Initializes modules in registration order. If one fails, the modules
        // already initialized are shut down in reverse and false is returned.
        bool initialize();

        // Advances all modules by one frame and counts the frame.
        void update(float deltaTime);

        // Shuts down initialized modules in reverse order. Safe to call twice.
        void shutdown();

        void requestExit() { m_exitRequested = true; }
        bool exitRequested() const { return m_exitRequested; }

        bool isInitialized() const { return m_initialized; }
        std::uint64_t frameCount() const { return m_frameCount; }
        std::size_t moduleCount() const { return m_modules.size(); }
        const EngineConfig& config() const { return m_config; }

    private:
        EngineConfig m_config;
        std::vector<std::unique_ptr<IModule>> m_modules;
        std::size_t m_initializedCount = 0; // modules [0, n) successfully initialized
        bool m_initialized = false;
        bool m_exitRequested = false;
        std::uint64_t m_frameCount = 0;
    };
}
