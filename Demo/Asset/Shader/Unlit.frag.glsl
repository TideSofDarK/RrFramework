#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

#include "Unlit.glsl"

void main()
{
    vec2 uv = in_uv;
    vec3 color = texture(u_texture[0], uv).rgb;
    out_color = vec4(color, 1.0f);
}
