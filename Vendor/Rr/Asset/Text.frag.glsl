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
    vec2 dxdy = fwidth(uv) * u_font.atlasSize;
    float v_weight = 0.0f;
    float distanceRange = 2.0f;
    // float dist = getDistance(uv) + min(v_weight, 0.5 – 1.0 / 2.0) - 0.5;
    float dist = getDistance(uv) + min(v_weight, 0.5f - 1.0f / distanceRange) - 0.5f;
    float opacity = clamp(dist * distanceRange / length(dxdy) + 0.5, 0.0, 1.0);

    // outScreen = vec4(v_rgba.rgb, opacity * v_rgba.a);

    out_color = vec4(vec3(1.0f), opacity);
    // out_color = vec4(texture(u_fontAtlas, vec2(in_uv.x, 1.0f - in_uv.y)).rgb, 0.5f);
}
