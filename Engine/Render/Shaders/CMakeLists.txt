# Compiles GLSL to SPIR-V at build time into one generated header.
# Requires Node.js and `npm --prefix tools/shaders ci` (one-time setup).

find_program(RAVEN_NODE_EXECUTABLE node REQUIRED)

set(RAVEN_SHADER_TOOL "${CMAKE_SOURCE_DIR}/tools/shaders/compile.cjs")
set(RAVEN_SHADER_DEPS_DIR "${CMAKE_SOURCE_DIR}/tools/shaders/node_modules/@webgpu/glslang")
if(NOT EXISTS "${RAVEN_SHADER_DEPS_DIR}")
    message(FATAL_ERROR "Shader compiler dependencies missing. Run: npm --prefix tools/shaders ci")
endif()

set(RAVEN_SHADER_GLSL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/glsl")
set(RAVEN_TRIANGLE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/Raven/Render/Shaders/TriangleShaders.h")

add_custom_command(
    OUTPUT "${RAVEN_TRIANGLE_HEADER}"
    COMMAND "${RAVEN_NODE_EXECUTABLE}" "${RAVEN_SHADER_TOOL}" "${RAVEN_TRIANGLE_HEADER}"
            "vertex|${RAVEN_SHADER_GLSL_DIR}/triangle.vert.glsl|kTriangleVert"
            "fragment|${RAVEN_SHADER_GLSL_DIR}/triangle.frag.glsl|kTriangleFrag"
    DEPENDS "${RAVEN_SHADER_TOOL}"
            "${RAVEN_SHADER_GLSL_DIR}/triangle.vert.glsl"
            "${RAVEN_SHADER_GLSL_DIR}/triangle.frag.glsl"
    COMMENT "Compiling GLSL shaders to SPIR-V"
    VERBATIM)

# The generated header is a source of this library so it is built before any consumer.
add_library(RavenShaders STATIC
    "${RAVEN_TRIANGLE_HEADER}"
    src/Shaders.cpp
)
target_include_directories(RavenShaders PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_source_files_properties("${RAVEN_TRIANGLE_HEADER}" PROPERTIES HEADER_FILE_ONLY TRUE)
