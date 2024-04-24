#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 in_position;

layout(location = 1) out vec2 out_uv;

#include "Text.glsl"

void main()
{
    const float size = 32.0f;
    vec2 basePosition = u_constants.positionScreenSpace;
    basePosition += in_position * size;
    basePosition.x += 48.0f * gl_InstanceIndex;
    basePosition /= u_globals.screenSize;
    basePosition *= 2.0f;
    basePosition -= 1.0f;

    gl_Position = vec4(basePosition, 0.0f, 1.0f);

    out_uv.x = 0.0f;
    out_uv.y = 0.0f;
}
