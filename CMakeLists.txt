cmake_minimum_required(VERSION 3.20)

project(RavenEngine
    VERSION 0.1.0
    DESCRIPTION "Raven Engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RAVEN_BUILD_SAMPLES "Build sample applications" ON)
option(RAVEN_BUILD_TESTS   "Build unit tests"          ON)
option(RAVEN_BUILD_RENDER  "Build the Vulkan renderer (needs Vulkan, Node.js)" ON)

# enable_testing() must come before any add_test() call in subdirectories.
if(RAVEN_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(Engine/Core)
add_subdirectory(Engine/World)

if(RAVEN_BUILD_RENDER)
    add_subdirectory(Engine/Render)
endif()

if(RAVEN_BUILD_SAMPLES)
    add_subdirectory(Samples/HelloCore)
    add_subdirectory(Samples/WorldCube)
    if(RAVEN_BUILD_RENDER)
        add_subdirectory(Samples/RenderTriangle)
    endif()
endif()

if(RAVEN_BUILD_TESTS)
    add_subdirectory(Tests/Core)
    add_subdirectory(Tests/World)
    if(RAVEN_BUILD_RENDER)
        add_subdirectory(Tests/Render)
    endif()
endif()
