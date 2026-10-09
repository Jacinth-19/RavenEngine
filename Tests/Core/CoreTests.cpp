// Dependency-free tests for RavenCore. Each CHECK failure is counted and
// reported; the process exits non-zero if any check fails.

#include "Raven/Core/Application.h"
#include "Raven/Core/Engine.h"
#include "Raven/Core/Logger.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;

#define CHECK(cond)                                                             \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

    // Records every lifecycle call into a shared log so ordering can be asserted.
    class RecordingModule final : public Raven::IModule
    {
    public:
        RecordingModule(std::string name, std::vector<std::string>& log, bool failInit = false)
            : m_name(std::move(name)), m_log(log), m_failInit(failInit) {}

        std::string name() const override { return m_name; }

        bool initialize() override
        {
            m_log.push_back(m_name + ".init");
            return !m_failInit;
        }

        void update(float dt) override
        {
            m_lastDt = dt;
            m_log.push_back(m_name + ".update");
        }

        void shutdown() override
        {
            m_log.push_back(m_name + ".shutdown");
        }

        float lastDt() const { return m_lastDt; }

    private:
        std::string m_name;
        std::vector<std::string>& m_log;
        bool m_failInit;
        float m_lastDt = 0.0f;
    };

    void testInitAndShutdownOrder()
    {
        std::vector<std::string> log;
        Raven::Engine engine;
        engine.registerModule(std::make_unique<RecordingModule>("A", log));
        engine.registerModule(std::make_unique<RecordingModule>("B", log));

        CHECK(engine.initialize());
        CHECK(engine.isInitialized());
        engine.shutdown();
        CHECK(!engine.isInitialized());

        const std::vector<std::string> expected = {"A.init", "B.init", "B.shutdown", "A.shutdown"};
        CHECK(log == expected);
    }

    void testUpdateForwardsDeltaAndCountsFrames()
    {
        std::vector<std::string> log;
        Raven::Engine engine;
        auto module = std::make_unique<RecordingModule>("A", log);
        RecordingModule* raw = module.get();
        engine.registerModule(std::move(module));
        CHECK(engine.initialize());

        engine.update(0.016f);
        engine.update(0.5f);
        CHECK(engine.frameCount() == 2);
        CHECK(raw->lastDt() == 0.5f);
        engine.shutdown();
    }

    void testFailedInitRollsBackOnlyInitializedModules()
    {
        std::vector<std::string> log;
        Raven::Engine engine;
        engine.registerModule(std::make_unique<RecordingModule>("A", log));
        engine.registerModule(std::make_unique<RecordingModule>("B", log, /*failInit=*/true));
        engine.registerModule(std::make_unique<RecordingModule>("C", log));

        CHECK(!engine.initialize());
        CHECK(!engine.isInitialized());

        // C must never be initialized or shut down; B failed so is not shut down.
        const std::vector<std::string> expected = {"A.init", "B.init", "A.shutdown"};
        CHECK(log == expected);
    }

    void testRegistrationRules()
    {
        std::vector<std::string> log;
        Raven::Engine engine;

        CHECK(!engine.registerModule(nullptr));
        CHECK(engine.registerModule(std::make_unique<RecordingModule>("A", log)));
        CHECK(!engine.registerModule(std::make_unique<RecordingModule>("A", log))); // duplicate name
        CHECK(engine.moduleCount() == 1);

        CHECK(engine.initialize());
        CHECK(!engine.registerModule(std::make_unique<RecordingModule>("B", log))); // after init
        CHECK(engine.moduleCount() == 1);
        engine.shutdown();
    }

    void testMaxFramesRequestsExit()
    {
        Raven::EngineConfig config;
        config.maxFrames = 3;
        Raven::Engine engine(config);
        std::vector<std::string> log;
        engine.registerModule(std::make_unique<RecordingModule>("A", log));
        CHECK(engine.initialize());

        engine.update(0.0f);
        engine.update(0.0f);
        CHECK(!engine.exitRequested());
        engine.update(0.0f);
        CHECK(engine.exitRequested());
        engine.shutdown();
    }

    void testApplicationRunsToMaxFrames()
    {
        Raven::EngineConfig config;
        config.maxFrames = 5;
        Raven::Application app(config);
        std::vector<std::string> log;
        app.engine().registerModule(std::make_unique<RecordingModule>("A", log));

        CHECK(app.run() == 0);
        CHECK(app.engine().frameCount() == 5);
        CHECK(!app.engine().isInitialized());
        CHECK(log.front() == "A.init");
        CHECK(log.back() == "A.shutdown");
    }
}

int main()
{
    Raven::Log::setLevel(Raven::LogLevel::Off); // keep test output clean

    testInitAndShutdownOrder();
    testUpdateForwardsDeltaAndCountsFrames();
    testFailedInitRollsBackOnlyInitializedModules();
    testRegistrationRules();
    testMaxFramesRequestsExit();
    testApplicationRunsToMaxFrames();

    if (g_failures == 0)
    {
        std::printf("RavenCore tests: all passed\n");
        return 0;
    }
    std::fprintf(stderr, "RavenCore tests: %d check(s) failed\n", g_failures);
    return 1;
}
