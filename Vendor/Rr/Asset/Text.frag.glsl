#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

#include "Text.glsl"

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

float getDistance(vec2 uv)
{
    vec3 msdf = texture(u_fontAtlas, uv).rgb;
    return median(msdf.r, msdf.g, msdf.b);
}

void main()
{
    vec2 uv = vec2(in_uv.x, 1.0f - in_uv.y);
    vec2 dxdy = fwidth(uv) * textureSize(u_fontAtlas, 0);
    float weight = 0.0f;
    float dist = getDistance(uv) + min(weight, 0.5f - 1.0f / u_font.distanceRange) - 0.5f;
    float opacity = clamp(dist * u_font.distanceRange / length(dxdy) + 0.5, 0.0, 1.0);

    out_color = vec4(vec3(1.0f), opacity);
}
