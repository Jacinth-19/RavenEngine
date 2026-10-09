#include "Raven/Render/Vulkan/VulkanRenderDevice.h"

#include "Raven/Core/Logger.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Raven::Render
{
    namespace
    {
        constexpr std::uint32_t kSpirvMagic = 0x07230203;
        constexpr VkFormat kColorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        constexpr std::uint32_t kBytesPerPixel = 4;
        constexpr std::uint32_t kInvalidMemoryType = std::numeric_limits<std::uint32_t>::max();

        VkFormat attributeFormat(std::uint32_t componentCount)
        {
            switch (componentCount)
            {
                case 1: return VK_FORMAT_R32_SFLOAT;
                case 2: return VK_FORMAT_R32G32_SFLOAT;
                case 3: return VK_FORMAT_R32G32B32_SFLOAT;
                case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                default: return VK_FORMAT_UNDEFINED;
            }
        }

        std::string vkError(VkResult result)
        {
            return "VkResult " + std::to_string(static_cast<int>(result));
        }

        class VulkanRenderDevice final : public IRenderDevice
        {
        public:
            VulkanRenderDevice() = default;
            ~VulkanRenderDevice() override;

            VulkanRenderDevice(const VulkanRenderDevice&) = delete;
            VulkanRenderDevice& operator=(const VulkanRenderDevice&) = delete;

            bool init(std::string& error);

            const char* backendName() const override { return "Vulkan"; }

            BufferHandle createVertexBuffer(const void* data, std::size_t sizeBytes) override;
            ShaderHandle createShader(const std::uint32_t* spirv, std::size_t wordCount) override;
            PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
            RenderTargetHandle createRenderTarget(std::uint32_t width, std::uint32_t height) override;

            void destroy(BufferHandle buffer) override;
            void destroy(ShaderHandle shader) override;
            void destroy(PipelineHandle pipeline) override;
            void destroy(RenderTargetHandle target) override;

            bool beginFrame() override;
            bool beginPass(RenderTargetHandle target, ClearColor clear) override;
            bool bindPipeline(PipelineHandle pipeline) override;
            bool bindVertexBuffer(BufferHandle buffer) override;
            bool draw(std::uint32_t vertexCount) override;
            bool endPass() override;
            bool endFrame() override;

            bool readPixels(RenderTargetHandle target, std::vector<std::uint8_t>& rgba) override;

        private:
            struct BufferRes
            {
                VkBuffer buffer = VK_NULL_HANDLE;
                VkDeviceMemory memory = VK_NULL_HANDLE;
            };

            struct ShaderRes
            {
                VkShaderModule module = VK_NULL_HANDLE;
            };

            struct PipelineRes
            {
                VkPipeline pipeline = VK_NULL_HANDLE;
            };

            struct TargetRes
            {
                std::uint32_t width = 0;
                std::uint32_t height = 0;
                VkImage image = VK_NULL_HANDLE;
                VkDeviceMemory imageMemory = VK_NULL_HANDLE;
                VkImageView view = VK_NULL_HANDLE;
                VkFramebuffer framebuffer = VK_NULL_HANDLE;
                VkBuffer readback = VK_NULL_HANDLE;
                VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
                bool rendered = false; // image is in TRANSFER_SRC_OPTIMAL after a pass
            };

            std::uint32_t findMemoryType(std::uint32_t typeBits, VkMemoryPropertyFlags flags) const;
            bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                              VkMemoryPropertyFlags memoryFlags,
                              VkBuffer& buffer, VkDeviceMemory& memory);
            bool submitAndWait();
            void releaseTarget(TargetRes& target);
            void releaseBuffer(BufferRes& buffer);

            VkInstance m_instance = VK_NULL_HANDLE;
            VkPhysicalDevice m_physical = VK_NULL_HANDLE;
            VkPhysicalDeviceMemoryProperties m_memoryProperties{};
            VkDevice m_device = VK_NULL_HANDLE;
            VkQueue m_queue = VK_NULL_HANDLE;
            std::uint32_t m_queueFamily = 0;
            VkCommandPool m_commandPool = VK_NULL_HANDLE;
            VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
            VkFence m_fence = VK_NULL_HANDLE;
            VkRenderPass m_renderPass = VK_NULL_HANDLE;
            VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
            std::string m_deviceName;

            std::map<std::uint32_t, BufferRes> m_buffers;
            std::map<std::uint32_t, ShaderRes> m_shaders;
            std::map<std::uint32_t, PipelineRes> m_pipelines;
            std::map<std::uint32_t, TargetRes> m_targets;
            std::uint32_t m_nextId = 1;

            bool m_inFrame = false;
            bool m_inPass = false;
            std::uint32_t m_passTarget = 0;
            bool m_pipelineBound = false;
            bool m_vertexBound = false;
        };

        VulkanRenderDevice::~VulkanRenderDevice()
        {
            if (m_device != VK_NULL_HANDLE)
            {
                vkDeviceWaitIdle(m_device);
                for (auto& entry : m_targets) releaseTarget(entry.second);
                for (auto& entry : m_buffers) releaseBuffer(entry.second);
                for (auto& entry : m_pipelines) vkDestroyPipeline(m_device, entry.second.pipeline, nullptr);
                for (auto& entry : m_shaders) vkDestroyShaderModule(m_device, entry.second.module, nullptr);
                vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
                vkDestroyRenderPass(m_device, m_renderPass, nullptr);
                vkDestroyFence(m_device, m_fence, nullptr);
                vkDestroyCommandPool(m_device, m_commandPool, nullptr);
                vkDestroyDevice(m_device, nullptr);
            }
            if (m_instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(m_instance, nullptr);
            }
        }

        bool VulkanRenderDevice::init(std::string& error)
        {
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.pApplicationName = "Raven";
            app.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            instanceInfo.pApplicationInfo = &app;

            if (VkResult r = vkCreateInstance(&instanceInfo, nullptr, &m_instance); r != VK_SUCCESS)
            {
                error = "vkCreateInstance failed (" + vkError(r) + "). Is a Vulkan loader and ICD installed?";
                return false;
            }

            // Pick a physical device with a graphics queue, preferring discrete GPUs.
            std::uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
            if (deviceCount == 0)
            {
                error = "no Vulkan physical devices found";
                return false;
            }
            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

            bool found = false;
            int bestScore = -1;
            for (VkPhysicalDevice candidate : devices)
            {
                std::uint32_t familyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
                std::vector<VkQueueFamilyProperties> families(familyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());

                for (std::uint32_t i = 0; i < familyCount; ++i)
                {
                    if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
                    {
                        continue;
                    }
                    VkPhysicalDeviceProperties props{};
                    vkGetPhysicalDeviceProperties(candidate, &props);
                    const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
                    if (score > bestScore)
                    {
                        bestScore = score;
                        m_physical = candidate;
                        m_queueFamily = i;
                        m_deviceName = props.deviceName;
                        found = true;
                    }
                    break;
                }
            }
            if (!found)
            {
                error = "no Vulkan device with a graphics queue";
                return false;
            }
            vkGetPhysicalDeviceMemoryProperties(m_physical, &m_memoryProperties);

            const float priority = 1.0f;
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex = m_queueFamily;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;

            VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            deviceInfo.queueCreateInfoCount = 1;
            deviceInfo.pQueueCreateInfos = &queueInfo;

            if (VkResult r = vkCreateDevice(m_physical, &deviceInfo, nullptr, &m_device); r != VK_SUCCESS)
            {
                error = "vkCreateDevice failed (" + vkError(r) + ")";
                return false;
            }
            vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

            VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = m_queueFamily;
            if (VkResult r = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool); r != VK_SUCCESS)
            {
                error = "vkCreateCommandPool failed (" + vkError(r) + ")";
                return false;
            }

            VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            allocInfo.commandPool = m_commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            if (VkResult r = vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer); r != VK_SUCCESS)
            {
                error = "vkAllocateCommandBuffers failed (" + vkError(r) + ")";
                return false;
            }

            VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            if (VkResult r = vkCreateFence(m_device, &fenceInfo, nullptr, &m_fence); r != VK_SUCCESS)
            {
                error = "vkCreateFence failed (" + vkError(r) + ")";
                return false;
            }

            // One render pass shared by all targets and pipelines. Its final layout
            // is TRANSFER_SRC so every target can be read back without extra barriers.
            VkAttachmentDescription color{};
            color.format = kColorFormat;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            color.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;

            VkSubpassDependency toTransfer{};
            toTransfer.srcSubpass = 0;
            toTransfer.dstSubpass = VK_SUBPASS_EXTERNAL;
            toTransfer.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            toTransfer.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            passInfo.attachmentCount = 1;
            passInfo.pAttachments = &color;
            passInfo.subpassCount = 1;
            passInfo.pSubpasses = &subpass;
            passInfo.dependencyCount = 1;
            passInfo.pDependencies = &toTransfer;
            if (VkResult r = vkCreateRenderPass(m_device, &passInfo, nullptr, &m_renderPass); r != VK_SUCCESS)
            {
                error = "vkCreateRenderPass failed (" + vkError(r) + ")";
                return false;
            }

            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            if (VkResult r = vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout); r != VK_SUCCESS)
            {
                error = "vkCreatePipelineLayout failed (" + vkError(r) + ")";
                return false;
            }

            Log::info("Vulkan device: " + m_deviceName);
            return true;
        }

        std::uint32_t VulkanRenderDevice::findMemoryType(std::uint32_t typeBits,
                                                         VkMemoryPropertyFlags flags) const
        {
            for (std::uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
            {
                const bool allowed = (typeBits & (1u << i)) != 0;
                const bool hasFlags = (m_memoryProperties.memoryTypes[i].propertyFlags & flags) == flags;
                if (allowed && hasFlags)
                {
                    return i;
                }
            }
            return kInvalidMemoryType;
        }

        bool VulkanRenderDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                              VkMemoryPropertyFlags memoryFlags,
                                              VkBuffer& buffer, VkDeviceMemory& memory)
        {
            VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            info.size = size;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            if (VkResult r = vkCreateBuffer(m_device, &info, nullptr, &buffer); r != VK_SUCCESS)
            {
                Log::error("vkCreateBuffer failed (" + vkError(r) + ")");
                return false;
            }

            VkMemoryRequirements req{};
            vkGetBufferMemoryRequirements(m_device, buffer, &req);
            const std::uint32_t type = findMemoryType(req.memoryTypeBits, memoryFlags);
            if (type == kInvalidMemoryType)
            {
                Log::error("no suitable memory type for buffer");
                vkDestroyBuffer(m_device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
                return false;
            }

            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize = req.size;
            alloc.memoryTypeIndex = type;
            if (VkResult r = vkAllocateMemory(m_device, &alloc, nullptr, &memory); r != VK_SUCCESS)
            {
                Log::error("vkAllocateMemory failed (" + vkError(r) + ")");
                vkDestroyBuffer(m_device, buffer, nullptr);
                buffer = VK_NULL_HANDLE;
                return false;
            }
            vkBindBufferMemory(m_device, buffer, memory, 0);
            return true;
        }

        bool VulkanRenderDevice::submitAndWait()
        {
            vkResetFences(m_device, 1, &m_fence);

            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &m_commandBuffer;
            if (VkResult r = vkQueueSubmit(m_queue, 1, &submit, m_fence); r != VK_SUCCESS)
            {
                Log::error("vkQueueSubmit failed (" + vkError(r) + ")");
                return false;
            }
            if (VkResult r = vkWaitForFences(m_device, 1, &m_fence, VK_TRUE,
                                             std::numeric_limits<std::uint64_t>::max());
                r != VK_SUCCESS)
            {
                Log::error("vkWaitForFences failed (" + vkError(r) + ")");
                return false;
            }
            return true;
        }

        void VulkanRenderDevice::releaseBuffer(BufferRes& buffer)
        {
            vkDestroyBuffer(m_device, buffer.buffer, nullptr);
            vkFreeMemory(m_device, buffer.memory, nullptr);
            buffer = BufferRes{};
        }

        void VulkanRenderDevice::releaseTarget(TargetRes& target)
        {
            vkDestroyFramebuffer(m_device, target.framebuffer, nullptr);
            vkDestroyImageView(m_device, target.view, nullptr);
            vkDestroyImage(m_device, target.image, nullptr);
            vkFreeMemory(m_device, target.imageMemory, nullptr);
            vkDestroyBuffer(m_device, target.readback, nullptr);
            vkFreeMemory(m_device, target.readbackMemory, nullptr);
            target = TargetRes{};
        }

        // Resources

        BufferHandle VulkanRenderDevice::createVertexBuffer(const void* data, std::size_t sizeBytes)
        {
            if (!data || sizeBytes == 0)
            {
                Log::error("createVertexBuffer: empty data");
                return {};
            }

            BufferRes res;
            const VkMemoryPropertyFlags hostVisible =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            if (!createBuffer(sizeBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hostVisible,
                              res.buffer, res.memory))
            {
                return {};
            }

            void* mapped = nullptr;
            if (vkMapMemory(m_device, res.memory, 0, sizeBytes, 0, &mapped) != VK_SUCCESS)
            {
                Log::error("vkMapMemory failed for vertex buffer");
                releaseBuffer(res);
                return {};
            }
            std::memcpy(mapped, data, sizeBytes);
            vkUnmapMemory(m_device, res.memory);

            const std::uint32_t id = m_nextId++;
            m_buffers.emplace(id, res);
            return BufferHandle{id};
        }

        ShaderHandle VulkanRenderDevice::createShader(const std::uint32_t* spirv, std::size_t wordCount)
        {
            if (!spirv || wordCount < 5 || spirv[0] != kSpirvMagic)
            {
                Log::error("createShader: not a valid SPIR-V module");
                return {};
            }

            VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
            info.codeSize = wordCount * sizeof(std::uint32_t);
            info.pCode = spirv;

            ShaderRes res;
            if (VkResult r = vkCreateShaderModule(m_device, &info, nullptr, &res.module); r != VK_SUCCESS)
            {
                Log::error("vkCreateShaderModule failed (" + vkError(r) + ")");
                return {};
            }

            const std::uint32_t id = m_nextId++;
            m_shaders.emplace(id, res);
            return ShaderHandle{id};
        }

        PipelineHandle VulkanRenderDevice::createGraphicsPipeline(const GraphicsPipelineDesc& desc)
        {
            const auto vs = m_shaders.find(desc.vertexShader.id);
            const auto fs = m_shaders.find(desc.fragmentShader.id);
            if (vs == m_shaders.end() || fs == m_shaders.end())
            {
                Log::error("createGraphicsPipeline: vertex or fragment shader is invalid");
                return {};
            }
            if (desc.vertexStride == 0 || desc.attributes.empty())
            {
                Log::error("createGraphicsPipeline: vertex layout is empty");
                return {};
            }

            std::vector<VkVertexInputAttributeDescription> attributes;
            attributes.reserve(desc.attributes.size());
            for (const VertexAttribute& attr : desc.attributes)
            {
                const VkFormat format = attributeFormat(attr.componentCount);
                if (format == VK_FORMAT_UNDEFINED)
                {
                    Log::error("createGraphicsPipeline: componentCount must be 1..4");
                    return {};
                }
                attributes.push_back(VkVertexInputAttributeDescription{attr.location, 0, format, attr.offset});
            }

            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = desc.vertexStride;
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkPipelineShaderStageCreateInfo stages[2]{};
            stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            stages[0].module = vs->second.module;
            stages[0].pName = "main";
            stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[1].module = fs->second.module;
            stages[1].pName = "main";

            VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            vertexInput.vertexBindingDescriptionCount = 1;
            vertexInput.pVertexBindingDescriptions = &binding;
            vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
            vertexInput.pVertexAttributeDescriptions = attributes.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

            // Viewport and scissor are dynamic so one pipeline works for any target size.
            VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewport.viewportCount = 1;
            viewport.scissorCount = 1;

            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

            VkPipelineColorBlendAttachmentState blendAttachment{};
            blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            blend.attachmentCount = 1;
            blend.pAttachments = &blendAttachment;

            const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamic.dynamicStateCount = 2;
            dynamic.pDynamicStates = dynamicStates;

            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            info.stageCount = 2;
            info.pStages = stages;
            info.pVertexInputState = &vertexInput;
            info.pInputAssemblyState = &inputAssembly;
            info.pViewportState = &viewport;
            info.pRasterizationState = &raster;
            info.pMultisampleState = &multisample;
            info.pColorBlendState = &blend;
            info.pDynamicState = &dynamic;
            info.layout = m_pipelineLayout;
            info.renderPass = m_renderPass;
            info.subpass = 0;

            PipelineRes res;
            if (VkResult r = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &info, nullptr, &res.pipeline);
                r != VK_SUCCESS)
            {
                Log::error("vkCreateGraphicsPipelines failed (" + vkError(r) + ")");
                return {};
            }

            const std::uint32_t id = m_nextId++;
            m_pipelines.emplace(id, res);
            return PipelineHandle{id};
        }

        RenderTargetHandle VulkanRenderDevice::createRenderTarget(std::uint32_t width, std::uint32_t height)
        {
            if (width == 0 || height == 0)
            {
                Log::error("createRenderTarget: size must be non-zero");
                return {};
            }

            TargetRes res;
            res.width = width;
            res.height = height;

            VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = kColorFormat;
            imageInfo.extent = {width, height, 1};
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            auto fail = [this, &res](const char* what, VkResult r) {
                Log::error(std::string("createRenderTarget: ") + what + " failed (" + vkError(r) + ")");
                releaseTarget(res);
                return RenderTargetHandle{};
            };

            if (VkResult r = vkCreateImage(m_device, &imageInfo, nullptr, &res.image); r != VK_SUCCESS)
            {
                return fail("vkCreateImage", r);
            }

            VkMemoryRequirements req{};
            vkGetImageMemoryRequirements(m_device, res.image, &req);
            const std::uint32_t type = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (type == kInvalidMemoryType)
            {
                Log::error("createRenderTarget: no device-local memory type");
                releaseTarget(res);
                return {};
            }
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize = req.size;
            alloc.memoryTypeIndex = type;
            if (VkResult r = vkAllocateMemory(m_device, &alloc, nullptr, &res.imageMemory); r != VK_SUCCESS)
            {
                return fail("vkAllocateMemory", r);
            }
            vkBindImageMemory(m_device, res.image, res.imageMemory, 0);

            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = res.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = kColorFormat;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            if (VkResult r = vkCreateImageView(m_device, &viewInfo, nullptr, &res.view); r != VK_SUCCESS)
            {
                return fail("vkCreateImageView", r);
            }

            VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbInfo.renderPass = m_renderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments = &res.view;
            fbInfo.width = width;
            fbInfo.height = height;
            fbInfo.layers = 1;
            if (VkResult r = vkCreateFramebuffer(m_device, &fbInfo, nullptr, &res.framebuffer); r != VK_SUCCESS)
            {
                return fail("vkCreateFramebuffer", r);
            }

            const VkDeviceSize readbackSize = static_cast<VkDeviceSize>(width) * height * kBytesPerPixel;
            const VkMemoryPropertyFlags hostVisible =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            if (!createBuffer(readbackSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, hostVisible,
                              res.readback, res.readbackMemory))
            {
                releaseTarget(res);
                return {};
            }

            const std::uint32_t id = m_nextId++;
            m_targets.emplace(id, res);
            return RenderTargetHandle{id};
        }

        void VulkanRenderDevice::destroy(BufferHandle buffer)
        {
            auto it = m_buffers.find(buffer.id);
            if (it == m_buffers.end()) return;
            vkDeviceWaitIdle(m_device);
            releaseBuffer(it->second);
            m_buffers.erase(it);
        }

        void VulkanRenderDevice::destroy(ShaderHandle shader)
        {
            auto it = m_shaders.find(shader.id);
            if (it == m_shaders.end()) return;
            vkDestroyShaderModule(m_device, it->second.module, nullptr);
            m_shaders.erase(it);
        }

        void VulkanRenderDevice::destroy(PipelineHandle pipeline)
        {
            auto it = m_pipelines.find(pipeline.id);
            if (it == m_pipelines.end()) return;
            vkDeviceWaitIdle(m_device);
            vkDestroyPipeline(m_device, it->second.pipeline, nullptr);
            m_pipelines.erase(it);
        }

        void VulkanRenderDevice::destroy(RenderTargetHandle target)
        {
            auto it = m_targets.find(target.id);
            if (it == m_targets.end()) return;
            vkDeviceWaitIdle(m_device);
            releaseTarget(it->second);
            m_targets.erase(it);
        }

        // Frame recording

        bool VulkanRenderDevice::beginFrame()
        {
            if (m_inFrame)
            {
                Log::error("beginFrame: a frame is already open");
                return false;
            }
            vkResetCommandBuffer(m_commandBuffer, 0);

            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (VkResult r = vkBeginCommandBuffer(m_commandBuffer, &begin); r != VK_SUCCESS)
            {
                Log::error("vkBeginCommandBuffer failed (" + vkError(r) + ")");
                return false;
            }
            m_inFrame = true;
            return true;
        }

        bool VulkanRenderDevice::beginPass(RenderTargetHandle target, ClearColor clear)
        {
            if (!m_inFrame || m_inPass)
            {
                Log::error("beginPass: requires an open frame and no active pass");
                return false;
            }
            auto it = m_targets.find(target.id);
            if (it == m_targets.end())
            {
                Log::error("beginPass: invalid render target");
                return false;
            }
            const TargetRes& t = it->second;

            VkClearValue clearValue{};
            clearValue.color.float32[0] = clear.r;
            clearValue.color.float32[1] = clear.g;
            clearValue.color.float32[2] = clear.b;
            clearValue.color.float32[3] = clear.a;

            VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            info.renderPass = m_renderPass;
            info.framebuffer = t.framebuffer;
            info.renderArea = {{0, 0}, {t.width, t.height}};
            info.clearValueCount = 1;
            info.pClearValues = &clearValue;
            vkCmdBeginRenderPass(m_commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{0.0f, 0.0f, static_cast<float>(t.width), static_cast<float>(t.height), 0.0f, 1.0f};
            VkRect2D scissor{{0, 0}, {t.width, t.height}};
            vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

            m_inPass = true;
            m_passTarget = target.id;
            m_pipelineBound = false;
            m_vertexBound = false;
            return true;
        }

        bool VulkanRenderDevice::bindPipeline(PipelineHandle pipeline)
        {
            if (!m_inPass)
            {
                Log::error("bindPipeline: outside a pass");
                return false;
            }
            auto it = m_pipelines.find(pipeline.id);
            if (it == m_pipelines.end())
            {
                Log::error("bindPipeline: invalid pipeline");
                return false;
            }
            vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, it->second.pipeline);
            m_pipelineBound = true;
            return true;
        }

        bool VulkanRenderDevice::bindVertexBuffer(BufferHandle buffer)
        {
            if (!m_inPass)
            {
                Log::error("bindVertexBuffer: outside a pass");
                return false;
            }
            auto it = m_buffers.find(buffer.id);
            if (it == m_buffers.end())
            {
                Log::error("bindVertexBuffer: invalid buffer");
                return false;
            }
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &it->second.buffer, &offset);
            m_vertexBound = true;
            return true;
        }

        bool VulkanRenderDevice::draw(std::uint32_t vertexCount)
        {
            if (!m_inPass || !m_pipelineBound || !m_vertexBound)
            {
                Log::error("draw: needs an active pass with a pipeline and vertex buffer bound");
                return false;
            }
            vkCmdDraw(m_commandBuffer, vertexCount, 1, 0, 0);
            return true;
        }

        bool VulkanRenderDevice::endPass()
        {
            if (!m_inPass)
            {
                Log::error("endPass: no active pass");
                return false;
            }
            vkCmdEndRenderPass(m_commandBuffer);
            m_inPass = false;

            auto it = m_targets.find(m_passTarget);
            if (it != m_targets.end())
            {
                it->second.rendered = true;
            }
            return true;
        }

        bool VulkanRenderDevice::endFrame()
        {
            if (!m_inFrame || m_inPass)
            {
                Log::error("endFrame: requires an open frame with no active pass");
                return false;
            }
            if (VkResult r = vkEndCommandBuffer(m_commandBuffer); r != VK_SUCCESS)
            {
                Log::error("vkEndCommandBuffer failed (" + vkError(r) + ")");
                m_inFrame = false;
                return false;
            }
            m_inFrame = false;
            return submitAndWait();
        }

        bool VulkanRenderDevice::readPixels(RenderTargetHandle target, std::vector<std::uint8_t>& rgba)
        {
            if (m_inFrame)
            {
                Log::error("readPixels: call after endFrame()");
                return false;
            }
            auto it = m_targets.find(target.id);
            if (it == m_targets.end())
            {
                Log::error("readPixels: invalid render target");
                return false;
            }
            const TargetRes& t = it->second;
            if (!t.rendered)
            {
                Log::error("readPixels: target has not been rendered to");
                return false;
            }

            vkResetCommandBuffer(m_commandBuffer, 0);
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(m_commandBuffer, &begin);

            VkBufferImageCopy region{};
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            region.imageExtent = {t.width, t.height, 1};
            vkCmdCopyImageToBuffer(m_commandBuffer, t.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   t.readback, 1, &region);

            VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = t.readback;
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 0, 0, nullptr, 1, &barrier, 0, nullptr);

            vkEndCommandBuffer(m_commandBuffer);
            if (!submitAndWait())
            {
                return false;
            }

            const std::size_t size = static_cast<std::size_t>(t.width) * t.height * kBytesPerPixel;
            void* mapped = nullptr;
            if (vkMapMemory(m_device, t.readbackMemory, 0, size, 0, &mapped) != VK_SUCCESS)
            {
                Log::error("readPixels: vkMapMemory failed");
                return false;
            }
            rgba.resize(size);
            std::memcpy(rgba.data(), mapped, size);
            vkUnmapMemory(m_device, t.readbackMemory);
            return true;
        }
    }

    std::unique_ptr<IRenderDevice> createVulkanRenderDevice(std::string& error)
    {
        auto device = std::make_unique<VulkanRenderDevice>();
        if (!device->init(error))
        {
            return nullptr;
        }
        return device;
    }
}
