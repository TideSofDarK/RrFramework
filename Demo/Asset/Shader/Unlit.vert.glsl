#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_u;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_v;
layout(location = 4) in vec4 in_tangent;

layout(location = 0) out vec2 out_uv;

#include "Unlit.glsl"

void main()
{
    gl_Position = u_globals.proj * u_globals.intermediate * u_globals.view * u_perDraw.model * vec4(in_position, 1.0f);

    out_uv.x = in_u;
    out_uv.y = in_v;
}
