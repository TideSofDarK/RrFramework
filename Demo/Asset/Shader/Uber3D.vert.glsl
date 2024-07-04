#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_u;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in float in_v;
layout(location = 4) in vec4 in_tangent;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;
layout(location = 3) out vec3 out_position;
layout(location = 4) out vec3 out_viewPosition;
layout(location = 5) out mat3 out_tbn;

#include "Uber3D.glsl"

void main()
{
    gl_Position = u_globals.proj * u_globals.view * u_perDraw.model * vec4(in_position, 1.0f);

    vec3 T = normalize(vec3(u_perDraw.model * vec4(in_tangent.xyz, 0.0)));
    vec3 N = normalize(vec3(u_perDraw.model * vec4(in_normal, 0.0)));
    vec3 B = cross(N, T);
    out_tbn = mat3(T, B, N);

    out_uv.x = in_u;
    out_uv.y = in_v;

    out_normal = processNormal(in_normal, u_perDraw.model);
    out_position = (u_perDraw.model * vec4(in_position, 1.0f)).xyz;
    out_viewPosition = u_globals.view[3].xyz;
}
