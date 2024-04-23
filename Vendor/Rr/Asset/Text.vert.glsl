#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 in_position;

layout(location = 1) out vec2 out_uv;

#include "Text.glsl"

void main()
{
    vec2 position = vec2(in_position);
    gl_Position = vec4(position, 0.0f, 1.0f);

    out_uv.x = 0.0f;
    out_uv.y = 0.0f;
}
