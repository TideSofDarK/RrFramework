#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_advance;
layout(location = 2) in uint in_glyphIndex;

layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec3 out_color;

#include "Text.glsl"

void main()
{
    const uint glyphIndexMask = ~0xFFE00000;
    const uint glyphColorMask = 0xF0000000;
    uint glyphIndex = in_glyphIndex & glyphIndexMask;
    uint glyphData = (in_glyphIndex & glyphColorMask) >> 28;
    out_color = u_globals.palette[glyphData].rgb;
    Glyph glyph = u_font.glyphs[glyphIndex];

    vec2 atlasSize = textureSize(u_fontAtlas, 0);

    const uint mask = 0x0000FFFF;
    float normalizedAtlasX = (float(((glyph.atlasXY & ~mask) >> 16)) + 0.5f) / atlasSize.x;
    float normalizedAtlasY = (float((glyph.atlasXY & mask)) + 0.5f) / atlasSize.y;
    float normalizedAtlasW = (float(((glyph.atlasWH & ~mask) >> 16))) / atlasSize.x;
    float normalizedAtlasH = (float((glyph.atlasWH & mask))) / atlasSize.y;

    vec2 planeLB = unpackHalf2x16(glyph.planeLB);
    vec2 planeRT = unpackHalf2x16(glyph.planeRT);

    vec2 glyphPosition = vec2(0.0f);
    glyphPosition.x += (1.0f - in_position.x) * planeLB.x;
    glyphPosition.x += (in_position.x) * planeRT.x;

    glyphPosition.y += (1.0f - in_position.y) * (1.0f - planeRT.y);
    glyphPosition.y += (in_position.y) * (1.0f - planeLB.y);

    float size = u_constants.size;
    // size += abs(cos((u_globals.time * 10.0f))) * 16.0f;

    vec2 basePosition = u_constants.positionScreenSpace;
    basePosition += glyphPosition * vec2(size);
    basePosition += in_advance * size;
    basePosition /= u_globals.screenSize;
    basePosition *= 2.0f;
    basePosition -= 1.0f;

    gl_Position = vec4(basePosition, 0.0f, 1.0f);

    float animationMask = step(1.0f, float(u_constants.flags & 1));
    gl_Position.y -= animationMask * cos((u_globals.time * 10.0f) + float(gl_InstanceIndex) * 1.5f) / 64.0f;

    out_uv.x = normalizedAtlasX + (normalizedAtlasW * in_position.x);
    out_uv.y = normalizedAtlasY + (normalizedAtlasH * (1.0f - in_position.y));
}
