#pragma once

// Render Hardware Interface (RHI). Backend-neutral. Implemented by RavenVulkan.
//
// Frame model:
//   beginFrame()
//     beginPass(target, clear)
//       bindPipeline / bindVertexBuffer / draw ...
//     endPass()
//   endFrame()          -> submits and waits for the GPU
//   readPixels(target)  -> copies a rendered target back to the CPU
//
// Resources are referenced by handles. A handle is invalid when id == 0 or
// after it has been destroyed.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Raven::Render
{
    template <typename Tag>
    struct Handle
    {
        std::uint32_t id = 0;

        bool valid() const { return id != 0; }
        friend bool operator==(Handle a, Handle b) { return a.id == b.id; }
        friend bool operator!=(Handle a, Handle b) { return !(a == b); }
    };

    struct BufferTag {};
    struct ShaderTag {};
    struct PipelineTag {};
    struct RenderTargetTag {};

    using BufferHandle = Handle<BufferTag>;
    using ShaderHandle = Handle<ShaderTag>;
    using PipelineHandle = Handle<PipelineTag>;
    using RenderTargetHandle = Handle<RenderTargetTag>;

    struct ClearColor
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    // One float attribute in the vertex stream. componentCount is 1..4.
    struct VertexAttribute
    {
        std::uint32_t location = 0;
        std::uint32_t componentCount = 0;
        std::uint32_t offset = 0; // bytes from the start of the vertex
    };

    struct GraphicsPipelineDesc
    {
        ShaderHandle vertexShader;
        ShaderHandle fragmentShader;
        std::uint32_t vertexStride = 0; // bytes per vertex
        std::vector<VertexAttribute> attributes;
    };

    class IRenderDevice
    {
    public:
        virtual ~IRenderDevice() = default;

        virtual const char* backendName() const = 0;

        // Resources. Creation returns an invalid handle on failure and logs why.
        virtual BufferHandle createVertexBuffer(const void* data, std::size_t sizeBytes) = 0;
        virtual ShaderHandle createShader(const std::uint32_t* spirv, std::size_t wordCount) = 0;
        virtual PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual RenderTargetHandle createRenderTarget(std::uint32_t width, std::uint32_t height) = 0;

        virtual void destroy(BufferHandle buffer) = 0;
        virtual void destroy(ShaderHandle shader) = 0;
        virtual void destroy(PipelineHandle pipeline) = 0;
        virtual void destroy(RenderTargetHandle target) = 0;

        // Frame recording
        virtual bool beginFrame() = 0;
        virtual bool beginPass(RenderTargetHandle target, ClearColor clear) = 0;
        virtual bool bindPipeline(PipelineHandle pipeline) = 0;
        virtual bool bindVertexBuffer(BufferHandle buffer) = 0;
        virtual bool draw(std::uint32_t vertexCount) = 0;
        virtual bool endPass() = 0;
        virtual bool endFrame() = 0;

        // Copies a rendered target to tightly packed RGBA8, top row first.
        // Must be called outside a frame. Waits for the GPU.
        virtual bool readPixels(RenderTargetHandle target, std::vector<std::uint8_t>& rgba) = 0;
    };
}
