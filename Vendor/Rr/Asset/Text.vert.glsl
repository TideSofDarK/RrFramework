#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 in_position;
layout(location = 1) in uint in_glyphIndex;

layout(location = 2) out vec2 out_uv;

#include "Text.glsl"

void main()
{
    Glyph glyph = u_font.glyphs[in_glyphIndex];

    vec2 atlasSize = textureSize(u_fontAtlas, 0);

    const uint mask = 0x0000FFFF;
    float normalizedAtlasX = (float(((glyph.atlasXY & ~mask) >> 16)) + 0.5f) / atlasSize.x;
    float normalizedAtlasY = (float((glyph.atlasXY & mask)) + 0.5f) / atlasSize.y;
    float normalizedAtlasW = (float(((glyph.atlasWH & ~mask) >> 16))) / atlasSize.x;
    float normalizedAtlasH = (float((glyph.atlasWH & mask))) / atlasSize.y;

    vec2 glyphPosition = vec2(0.0f);
    glyphPosition.x += (1.0f - in_position.x) * glyph.left;
    glyphPosition.x += (in_position.x) * (glyph.right);

    glyphPosition.y += (1.0f - in_position.y) * (1.0f - glyph.top);
    glyphPosition.y += (in_position.y) * ((1.0f - glyph.bottom));

    const float size = 32.0f;
    const float lineHeight = 1.2f;
    const float glyphAspect = normalizedAtlasW / normalizedAtlasH;
    vec2 basePosition = u_constants.positionScreenSpace;
    basePosition += glyphPosition * vec2(size, size);
    basePosition.x += size * glyph.advance * gl_InstanceIndex;
    basePosition /= u_globals.screenSize;
    basePosition *= 2.0f;
    basePosition -= 1.0f;

    gl_Position = vec4(basePosition, 0.0f, 1.0f);

    // gl_Position.y += cos((u_globals.time * 10.0f) + float(gl_InstanceIndex) * 1.5f) / 64.0f;

    out_uv.x = normalizedAtlasX + (normalizedAtlasW * in_position.x);
    out_uv.y = normalizedAtlasY + (normalizedAtlasH * (1.0f - in_position.y));
}
