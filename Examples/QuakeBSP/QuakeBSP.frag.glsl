#version 450

layout(location = 0) in vec3 InNormal;
layout(location = 1) in vec2 InTexCoord;
layout(location = 2) in flat uint InTexIndex;

layout(location = 0) out vec4 OutColor;

struct SGPUTexture
{
    vec2 AtlasTexCoord;
    vec2 AtlasTexSize;
};

layout(set = 0, binding = 1) uniform sampler2D AtlasImage;
layout(set = 0, binding = 3) readonly buffer UTextures
{
    SGPUTexture Textures[];
};

void main()
{
    SGPUTexture Tex = Textures[InTexIndex];
    vec2 TexSize = vec2(textureSize(AtlasImage, 0)) * Tex.AtlasTexSize;
    OutColor = texture(AtlasImage, Tex.AtlasTexCoord + fract(InTexCoord / TexSize) * Tex.AtlasTexSize);
}
