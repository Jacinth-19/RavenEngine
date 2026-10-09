# RavenRender: backend-neutral render interface (header-only).
add_library(RavenRender INTERFACE)
target_include_directories(RavenRender INTERFACE include)
target_link_libraries(RavenRender INTERFACE RavenCore)
target_compile_features(RavenRender INTERFACE cxx_std_17)

find_package(Vulkan REQUIRED)

add_subdirectory(Shaders)
add_subdirectory(Vulkan)
