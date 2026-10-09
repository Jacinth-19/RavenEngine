// Phase 3 smoke test: renders a colored triangle offscreen with Vulkan, reads the
// pixels back, checks them, and writes a PNG.
//
// Usage: RavenRenderTriangle <output.png>
// Exit codes: 0 ok, 1 failure, 77 skipped (no Vulkan device).

#include "PngWriter.h"

#include "Raven/Core/Logger.h"
#include "Raven/Render/RenderDevice.h"
#include "Raven/Render/Vulkan/VulkanRenderDevice.h"
#include "Raven/Render/Shaders/TriangleShaders.h"

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    constexpr std::uint32_t kSize = 256;
    constexpr std::uint8_t kBackground[4] = {26, 26, 31, 255}; // clear colour (0.1, 0.1, 0.12, 1)

    // Interleaved vertices: position (x, y), colour (r, g, b).
    const float kVertices[] = {
         0.0f, -0.6f,   1.0f, 0.0f, 0.0f,
         0.6f,  0.6f,   0.0f, 1.0f, 0.0f,
        -0.6f,  0.6f,   0.0f, 0.0f, 1.0f,
    };
    constexpr std::uint32_t kVertexCount = 3;
    constexpr std::uint32_t kVertexStride = 5 * sizeof(float);

    const std::uint8_t* pixel(const std::vector<std::uint8_t>& rgba, std::uint32_t x, std::uint32_t y)
    {
        return rgba.data() + (static_cast<std::size_t>(y) * kSize + x) * 4;
    }

    bool matchesBackground(const std::uint8_t* p)
    {
        for (int i = 0; i < 4; ++i)
        {
            const int diff = static_cast<int>(p[i]) - static_cast<int>(kBackground[i]);
            if (diff < -2 || diff > 2) return false;
        }
        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: RavenRenderTriangle <output.png>\n");
        return 1;
    }

    std::string error;
    auto device = Raven::Render::createVulkanRenderDevice(error);
    if (!device)
    {
        std::printf("SKIPPED: %s\n", error.c_str());
        return 77;
    }
    std::printf("backend: %s\n", device->backendName());

    using namespace Raven::Render;

    const BufferHandle vertices = device->createVertexBuffer(kVertices, sizeof(kVertices));
    const ShaderHandle vs = device->createShader(Shaders::kTriangleVert, std::size(Shaders::kTriangleVert));
    const ShaderHandle fs = device->createShader(Shaders::kTriangleFrag, std::size(Shaders::kTriangleFrag));

    GraphicsPipelineDesc desc;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;
    desc.vertexStride = kVertexStride;
    desc.attributes = {{0, 2, 0}, {1, 3, 2 * sizeof(float)}};
    const PipelineHandle pipeline = device->createGraphicsPipeline(desc);

    const RenderTargetHandle target = device->createRenderTarget(kSize, kSize);
    if (!vertices.valid() || !vs.valid() || !fs.valid() || !pipeline.valid() || !target.valid())
    {
        std::fprintf(stderr, "resource creation failed\n");
        return 1;
    }

    const ClearColor clear{0.1f, 0.1f, 0.12f, 1.0f};
    bool ok = device->beginFrame();
    ok = ok && device->beginPass(target, clear);
    ok = ok && device->bindPipeline(pipeline);
    ok = ok && device->bindVertexBuffer(vertices);
    ok = ok && device->draw(kVertexCount);
    ok = ok && device->endPass();
    ok = ok && device->endFrame();

    std::vector<std::uint8_t> rgba;
    ok = ok && device->readPixels(target, rgba);
    if (!ok || rgba.size() != static_cast<std::size_t>(kSize) * kSize * 4)
    {
        std::fprintf(stderr, "render or readback failed\n");
        return 1;
    }

    // Corner is background; the centre-lower area is inside the triangle.
    const bool cornerOk = matchesBackground(pixel(rgba, 0, 0));
    const bool insideOk = !matchesBackground(pixel(rgba, kSize / 2, kSize * 6 / 10));
    std::printf("corner background: %s\n", cornerOk ? "ok" : "FAIL");
    std::printf("triangle interior: %s\n", insideOk ? "ok" : "FAIL");

    if (!PngWriter::write(argv[1], kSize, kSize, rgba))
    {
        std::fprintf(stderr, "failed to write %s\n", argv[1]);
        return 1;
    }
    std::printf("wrote %s (%ux%u)\n", argv[1], kSize, kSize);

    device->destroy(pipeline);
    device->destroy(target);
    device->destroy(vertices);
    device->destroy(vs);
    device->destroy(fs);

    return (cornerOk && insideOk) ? 0 : 1;
}
