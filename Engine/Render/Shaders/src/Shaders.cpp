// Anchors the generated shader header in a translation unit so the library
// has a compiled object and consumers get ordered builds.
#include "Raven/Render/Shaders/TriangleShaders.h"

namespace Raven::Render::Shaders
{
    static_assert(sizeof(kTriangleVert) > 0, "triangle vertex shader is empty");
    static_assert(sizeof(kTriangleFrag) > 0, "triangle fragment shader is empty");
}
