add_executable(RavenRenderTests RenderTests.cpp)
target_link_libraries(RavenRenderTests PRIVATE RavenVulkan RavenShaders)
add_test(NAME RavenRenderTests COMMAND RavenRenderTests)
# Exits 77 (skipped) when no Vulkan device is available.
set_tests_properties(RavenRenderTests PROPERTIES SKIP_RETURN_CODE 77)
