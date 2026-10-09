add_library(RavenWorld STATIC
    src/World.cpp
    src/Scene.cpp
)

target_include_directories(RavenWorld
    PUBLIC  include
)

target_link_libraries(RavenWorld
    PUBLIC  RavenCore
)

target_compile_features(RavenWorld PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(RavenWorld PRIVATE /W4 /permissive-)
else()
    target_compile_options(RavenWorld PRIVATE -Wall -Wextra -Wpedantic)
endif()
