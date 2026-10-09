add_library(RavenCore STATIC
    src/Logger.cpp
    src/Engine.cpp
    src/Application.cpp
)

add_library(Raven::Core ALIAS RavenCore)

target_include_directories(RavenCore
    PUBLIC  include
)

target_compile_features(RavenCore PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(RavenCore PRIVATE /W4 /permissive-)
else()
    target_compile_options(RavenCore PRIVATE -Wall -Wextra -Wpedantic)
endif()
