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

add_subdirectory(Engine/Core)

if(RAVEN_BUILD_SAMPLES)
    add_subdirectory(Samples/HelloCore)
endif()

if(RAVEN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests/Core)
endif()
