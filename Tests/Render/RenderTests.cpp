// Vulkan render-device tests. Exit 77 (skipped) when no Vulkan device exists.

#include "Raven/Core/Logger.h"
#include "Raven/Render/RenderDevice.h"
#include "Raven/Render/Shaders/TriangleShaders.h"
#include "Raven/Render/Vulkan/VulkanRenderDevice.h"

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            ++g_failures;                                                              \
        }                                                                              \
    } while (0)

    using namespace Raven::Render;

    const float kTriangle[] = {
         0.0f, -0.6f,   1.0f, 0.0f, 0.0f,
         0.6f,  0.6f,   0.0f, 1.0f, 0.0f,
        -0.6f,  0.6f,   0.0f, 0.0f, 1.0f,
    };

    GraphicsPipelineDesc triangleDesc(ShaderHandle vs, ShaderHandle fs)
    {
        GraphicsPipelineDesc desc;
        desc.vertexShader = vs;
        desc.fragmentShader = fs;
        desc.vertexStride = 5 * sizeof(float);
        desc.attributes = {{0, 2, 0}, {1, 3, 2 * sizeof(float)}};
        return desc;
    }

    void testTriangleRendersExpectedPixels(IRenderDevice& device)
    {
        const std::uint32_t size = 64;
        BufferHandle vb = device.createVertexBuffer(kTriangle, sizeof(kTriangle));
        ShaderHandle vs = device.createShader(Shaders::kTriangleVert, std::size(Shaders::kTriangleVert));
        ShaderHandle fs = device.createShader(Shaders::kTriangleFrag, std::size(Shaders::kTriangleFrag));
        PipelineHandle pipe = device.createGraphicsPipeline(triangleDesc(vs, fs));
        RenderTargetHandle rt = device.createRenderTarget(size, size);
        CHECK(vb.valid() && vs.valid() && fs.valid() && pipe.valid() && rt.valid());

        // Before rendering, readback must be refused.
        std::vector<std::uint8_t> pixels;
        CHECK(!device.readPixels(rt, pixels));

        const ClearColor clear{0.0f, 0.0f, 0.0f, 1.0f};
        CHECK(device.beginFrame());
        CHECK(device.beginPass(rt, clear));
        CHECK(device.bindPipeline(pipe));
        CHECK(device.bindVertexBuffer(vb));
        CHECK(device.draw(3));
        CHECK(device.endPass());
        CHECK(device.endFrame());

        CHECK(device.readPixels(rt, pixels));
        CHECK(pixels.size() == size * size * 4);
        if (pixels.size() == size * size * 4)
        {
            // Top-left corner is outside the triangle: exactly the clear colour.
            CHECK(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 0 && pixels[3] == 255);

            // Point (0.0, 0.2) in NDC is inside the triangle: not the clear colour.
            const std::size_t idx = (static_cast<std::size_t>(size * 6 / 10) * size + size / 2) * 4;
            const bool notBlack = pixels[idx] + pixels[idx + 1] + pixels[idx + 2] > 0;
            CHECK(notBlack);
        }

        device.destroy(pipe);
        device.destroy(rt);
        device.destroy(vb);
        device.destroy(vs);
        device.destroy(fs);
    }

    void testInvalidShaderIsRejected(IRenderDevice& device)
    {
        const std::uint32_t notSpirv[] = {0x12345678, 0, 0, 0, 0};
        CHECK(!device.createShader(notSpirv, 5).valid());
        CHECK(!device.createShader(nullptr, 0).valid());
    }

    void testInvalidHandlesAreRejected(IRenderDevice& device)
    {
        CHECK(!device.createVertexBuffer(nullptr, 16).valid());
        CHECK(!device.createRenderTarget(0, 16).valid());

        CHECK(!device.beginPass(RenderTargetHandle{}, ClearColor{}));
        CHECK(!device.bindPipeline(PipelineHandle{12345}));
        CHECK(!device.draw(3));          // no frame open
        CHECK(!device.endFrame());       // no frame open
    }

    void testPipelineWithBadShaderIsRejected(IRenderDevice& device)
    {
        GraphicsPipelineDesc desc = triangleDesc(ShaderHandle{}, ShaderHandle{});
        CHECK(!device.createGraphicsPipeline(desc).valid());
    }
}

int main()
{
    Raven::Log::setLevel(Raven::LogLevel::Off);

    std::string error;
    auto device = createVulkanRenderDevice(error);
    if (!device)
    {
        std::printf("SKIPPED: %s\n", error.c_str());
        return 77;
    }

    testTriangleRendersExpectedPixels(*device);
    testInvalidShaderIsRejected(*device);
    testInvalidHandlesAreRejected(*device);
    testPipelineWithBadShaderIsRejected(*device);

    if (g_failures == 0)
    {
        std::printf("RavenRender tests (%s): all passed\n", device->backendName());
        return 0;
    }
    std::fprintf(stderr, "RavenRender tests: %d check(s) failed\n", g_failures);
    return 1;
}
