// Phase 1 smoke test: an empty application with one module that logs its
// lifecycle. Runs headless for a fixed number of frames.

#include "Raven/Core/Application.h"
#include "Raven/Core/Logger.h"

#include <memory>
#include <string>

namespace
{
    class HelloModule final : public Raven::IModule
    {
    public:
        std::string name() const override { return "Hello"; }

        bool initialize() override
        {
            Raven::Log::info("Hello: initialize");
            return true;
        }

        void update(float dt) override
        {
            (void)dt;
        }

        void shutdown() override
        {
            Raven::Log::info("Hello: shutdown");
        }
    };
}

int main()
{
    Raven::EngineConfig config;
    config.appName = "HelloCore";
    config.maxFrames = 3;

    Raven::Application app(config);
    app.engine().registerModule(std::make_unique<HelloModule>());

    return app.run();
}
