#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_u;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_v;
layout(location = 4) in vec4 in_tangent;

/* Set 0 */
layout(set = 0, binding = 0) uniform Globals {
    mat4 view;
    mat4 intermediate;
    mat4 proj;
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
    mat4 reserved;
} u_constants;

void main()
{
    gl_Position = u_globals.proj * u_globals.intermediate * u_globals.view * u_draw.model * vec4(in_position, 1.0f);
}
