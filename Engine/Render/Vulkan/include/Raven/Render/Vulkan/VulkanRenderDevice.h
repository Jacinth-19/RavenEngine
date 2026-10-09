#pragma once

#include "Raven/Render/RenderDevice.h"

#include <memory>
#include <string>

namespace Raven::Render
{
    // Creates a Vulkan-backed device. Returns nullptr and fills `error` when no
    // usable Vulkan device exists (no loader, no ICD, no queue with graphics).
    std::unique_ptr<IRenderDevice> createVulkanRenderDevice(std::string& error);
}
