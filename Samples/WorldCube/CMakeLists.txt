add_executable(RavenWorldCube main.cpp)
target_link_libraries(RavenWorldCube PRIVATE RavenWorld)

# Round-trip across two separate processes: save a cube, then reopen and find it.
if(RAVEN_BUILD_TESTS)
    add_test(NAME RavenWorldCube.Write COMMAND RavenWorldCube write cube_scene.rscene)
    add_test(NAME RavenWorldCube.Read  COMMAND RavenWorldCube read  cube_scene.rscene)
    set_tests_properties(RavenWorldCube.Read PROPERTIES DEPENDS RavenWorldCube.Write)
endif()
