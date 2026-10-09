#pragma once

#include <string>

namespace Raven
{
    // A Module is a unit of engine functionality (renderer, physics, audio, ...).
    // The Engine owns modules and drives their lifecycle:
    //   initialize()  once, in registration order
    //   update(dt)    every frame, in registration order
    //   shutdown()    once, in reverse registration order
    class IModule
    {
    public:
        virtual ~IModule() = default;

        // Unique name used for lookup and diagnostics.
        virtual std::string name() const = 0;

        // Return false to abort engine startup.
        virtual bool initialize() = 0;

        virtual void update(float deltaTime) = 0;

        virtual void shutdown() = 0;
    };
}
