#include "Shared.glsl"

/* Set 0 */
layout(set = 0, binding = 0) uniform Globals {
    mat4 view;
    mat4 intermediate;
    mat4 proj;
    vec4 ambientLightColor;
    vec4 directionalLightDirection;
    vec4 directionalLightColor;
    vec4 directionalLightIntensity;
} u_globals;
layout(set = 0, binding = 1) uniform sampler2D u_prerenderedDepth;

/* Set 1 */
layout(set = 1, binding = 0) uniform Material {
    vec3 emissive;
} u_material;
layout(set = 1, binding = 1) uniform sampler2D u_texture[4];

/* Set 2 */
layout(set = 2, binding = 0) uniform Draw {
    mat4 model;
} u_draw;

layout(push_constant) uniform Constants
{
    mat4 Reserved;
} u_constants;
