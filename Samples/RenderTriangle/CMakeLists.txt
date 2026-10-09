add_executable(RavenRenderTriangle main.cpp)
target_link_libraries(RavenRenderTriangle PRIVATE RavenVulkan RavenShaders)
add_dependencies(RavenRenderTriangle RavenShaders)

if(RAVEN_BUILD_TESTS)
    # Exits 77 (skipped) when no Vulkan device is available.
    add_test(NAME RavenRenderTriangle COMMAND RavenRenderTriangle triangle.png)
    set_tests_properties(RavenRenderTriangle PROPERTIES SKIP_RETURN_CODE 77)
endif()
