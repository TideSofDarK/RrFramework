#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_color;

#include "Uber3D.glsl"

void main()
{
    // outFragColor = vec4(mix(inColor, ub_sceneData.ambientColor.rgb, ub_sceneData.ambientColor.a), 1.0f);
    // vec3 color = vec3(1.0f);
    // out_color = vec4(color, 1.0f);
    vec3 color = texture(u_texture[0], vec2(in_uv.x, 1.0f - in_uv.y)).rgb;
    color *= -dot(in_normal, vec3(0.0, 0.0, -1.0));
    out_color = vec4(color, 1.0f);

    // out_color = vec4(texture(u_prerenderedDepth, in_UV).r, 0.0f, 0.0f, 1.0f);

    color = vec3(1.0f, 1.0f, 1.0f);
    color *= -dot(in_normal, vec3(0.0, 0.0, -1.0));
    out_color = vec4(color, 1.0f);

    vec2 uv = in_uv;
    uv.y = 1.0 - uv.y;
    out_color = vec4(texture(u_texture[0], uv).rgb, 1.0);
}
