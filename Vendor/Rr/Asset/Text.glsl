#include "Shared.glsl"

struct Glyph {
    uint atlasXY;
    uint atlasWH;
    uint planeLB;
    uint planeRT;
};

/* Set 0 */
layout(set = 0, binding = 0) uniform Globals {
    float time;
    float reserved;
    vec2 screenSize;
} u_globals;

/* Set 1 */
layout(set = 1, binding = 0) uniform Font {
    float advance;
    float distanceRange;
    vec2 atlasSize;
    Glyph glyphs[2048];
} u_font;
layout(set = 1, binding = 1) uniform sampler2D u_fontAtlas;

/* Push Constants */
layout(push_constant) uniform Constants
{
    vec2 positionScreenSpace;
} u_constants;
