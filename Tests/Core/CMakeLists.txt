add_executable(RavenCoreTests CoreTests.cpp)
target_link_libraries(RavenCoreTests PRIVATE Raven::Core)
add_test(NAME RavenCoreTests COMMAND RavenCoreTests)
