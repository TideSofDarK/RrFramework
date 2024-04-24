#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

#include "Text.glsl"

void main()
{
    out_color = vec4(texture(u_fontAtlas, vec2(in_uv.x, 1.0f - in_uv.y)).rgb, 0.5f);
}
