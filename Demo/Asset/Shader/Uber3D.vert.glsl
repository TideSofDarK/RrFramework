#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_u;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_v;
layout(location = 4) in vec4 in_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_position;
layout(location = 4) out vec3 out_viewPosition;

#include "Uber3D.glsl"

void main()
{
    gl_Position = u_globals.proj * u_globals.intermediate * u_globals.view * u_draw.model * vec4(in_position, 1.0f);

    out_color = in_color;
    out_uv.x = in_u;
    out_uv.y = in_v;

    out_normal = processNormal(in_normal, u_draw.model);
    out_position = (u_draw.model * vec4(in_position, 1.0f)).xyz;
    out_viewPosition = u_globals.view[3].xyz;
}
