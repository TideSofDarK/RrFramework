#include "Shared.glsl"

struct Glyph {
    float normalizedWidth;
    float normalizedHeight;
    float normalizedX;
    float normalizedY;
};

/* Set 0 */
layout(set = 0, binding = 0) uniform Globals {
    float screenSize;
    float atlasSize;
} u_globals;
layout(set = 0, binding = 1) uniform sampler2D u_fontAtlas;

/* Set 1 */
layout(set = 1, binding = 0) uniform Custom {
    vec3 Reserved;
} u_material;

/* Push Constants */
layout(push_constant) uniform Constants
{
    vec2 positionScreenSpace;

} u_constants;
