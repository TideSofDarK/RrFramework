#version 450

layout(location = 0) in vec3 InNormal;
layout(location = 1) in vec2 InTexCoord;
layout(location = 2) in flat uint InTexIndex;
layout(location = 3) in flat uint InFaceIndex;
layout(location = 4) in vec2 InLightmapTexCoord;

layout(location = 0) out vec4 OutColor;

struct SGPUTexture
{
    vec2 AtlasTexCoord;
    vec2 AtlasTexSize;
};

struct SGPUFace
{
    ivec4 Lights;
    ivec2 LightmapSize;
    int DataOffset;
    int Unused0;
    vec2 MinUV;
    vec2 MaxUV;
    vec2 MidPolyUV;
    float Unused1;
    float Unused2;
};

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
    float Time;
};
layout(set = 0, binding = 2) readonly buffer UTextures
{
    SGPUTexture Textures[];
};
layout(set = 0, binding = 3) readonly buffer ULightmaps
{
    float Lightmaps[];
};
layout(set = 0, binding = 4) readonly buffer UFaces
{
    SGPUFace Faces[];
};
layout(set = 1, binding = 0) uniform sampler2D AtlasImage;

vec4 SampleAtlasTexture(SGPUTexture Tex, sampler2D Image, float Mul)
{
    vec2 TexSize = vec2(textureSize(Image, 0)) * Tex.AtlasTexSize;

    return texture(Image, Tex.AtlasTexCoord + fract(InTexCoord / TexSize) * Tex.AtlasTexSize);
}

float LightAnimationForType(int Type)
{
    if (Type == 10)
    {
        return abs(cos(Time * 5.0));
    }

    return 1.0;
}

float SampleLightmapOnce(vec2 TexCoord)
{
    SGPUFace Face = Faces[InFaceIndex];
    vec2 Size = Face.LightmapSize;

    ivec2 TexCoordI = ivec2(TexCoord);
    TexCoordI = clamp(TexCoordI, ivec2(0), Face.LightmapSize - 1);

    int BaseOffset = TexCoordI.y * int(Size.x) + TexCoordI.x;
    int LightmapDataSize = Face.LightmapSize.x * Face.LightmapSize.y;
    int LightmapIndex = 0;
    float Result = 0.0f;

    for (int Index = 0; Index < 4; ++Index)
    {
        if (Face.Lights[Index] != 255)
        {
            Result += LightAnimationForType(Face.Lights[Index]) *
                    Lightmaps[Face.DataOffset + (LightmapIndex * LightmapDataSize) + BaseOffset];

            LightmapIndex++;
        }
    }

    return Result;
}

float SampleLightmap()
{
    SGPUFace Face = Faces[InFaceIndex];
    if (Face.DataOffset == -1)
    {
        /* No lightmap! */

        if (Face.Lights[0] == 255)
        {
            return 0.0;
        }

        return 1.0;
    }

    vec2 Size = Face.LightmapSize;
    vec2 TexCoord = InLightmapTexCoord;
    TexCoord -= 0.5;
    vec2 Blend = fract(TexCoord);

    float ResultX0 = SampleLightmapOnce(TexCoord + vec2(0.0));
    float ResultX1 = SampleLightmapOnce(TexCoord + vec2(1.0, 0.0));
    float ResultY0 = SampleLightmapOnce(TexCoord + vec2(0.0, 1.0));
    float ResultY1 = SampleLightmapOnce(TexCoord + vec2(1.0, 1.0));

    float ResultX = mix(ResultX0, ResultX1, Blend.x);
    float ResultY = mix(ResultY0, ResultY1, Blend.x);
    float Result = mix(ResultX, ResultY, Blend.y);

    return Result;
}

void main()
{
    OutColor = SampleAtlasTexture(Textures[InTexIndex], AtlasImage, 1.0);
    OutColor.rgb *= SampleLightmap();

    OutColor.rgb *= 63;
    OutColor.rgb = floor(OutColor.rgb);
    OutColor.rgb /= 63;
}
