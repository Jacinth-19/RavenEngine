add_library(RavenVulkan STATIC
    src/VulkanRenderDevice.cpp
)

target_include_directories(RavenVulkan PUBLIC include)
target_link_libraries(RavenVulkan PUBLIC RavenRender Vulkan::Vulkan)

if(MSVC)
    target_compile_options(RavenVulkan PRIVATE /W4 /permissive-)
else()
    # Vulkan structs are initialised as `VkX info{VK_STRUCTURE_TYPE_X}`, which
    # zero-fills the rest by design. -Wmissing-field-initializers flags that idiom.
    target_compile_options(RavenVulkan PRIVATE -Wall -Wextra -Wpedantic
                           -Wno-missing-field-initializers)
endif()
