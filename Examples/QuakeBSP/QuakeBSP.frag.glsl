#version 450

layout(location = 0) in vec3 InNormal;
layout(location = 1) in vec2 InTexCoord;
layout(location = 2) in vec2 InLightmapTexCoord;
layout(location = 3) in flat uint InSurfaceIndex;
layout(location = 4) in flat uint InTextureIndex;
layout(location = 5) in flat uint InFaceIndex;

layout(location = 0) out vec4 OutColor;

struct SGPUSurface
{
    vec3 VectorX;
    float DistanceX;
    vec3 VectorY;
    float DistanceY;
    int Sky;
    int Water;
    int Unused1;
    int Unused2;
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
    float Anim0;
    float Anim1;
    float Anim2;
};
layout(set = 0, binding = 1) readonly buffer USurfaces
{
    SGPUSurface Surfaces[];
};
layout(set = 0, binding = 2) readonly buffer UTextures
{
    ivec4 Textures[];
};
layout(set = 0, binding = 3) readonly buffer ULightmaps
{
    float Lightmaps[];
};
layout(set = 0, binding = 4) readonly buffer UFaces
{
    SGPUFace Faces[];
};
layout(set = 1, binding = 0, rgba8) readonly uniform image2D ColormapImage;
layout(set = 1, binding = 1, r8ui) readonly uniform uimage2D AtlasImage;

uint SampleAtlasTexture(ivec4 Texture, vec2 TexCoordF)
{
    ivec2 Offset = Texture.xy;
    ivec2 Size = Texture.zw;

    SGPUSurface Surface = Surfaces[InSurfaceIndex];
    if (Surface.Water != 0)
    {
        const float ANIM0 = 2.5;
        const float ANIM1 = 9.5;
        TexCoordF.x += sin(TexCoordF.y / float(Size.y / ANIM0) + Time) * ANIM1;
        TexCoordF.y += sin(TexCoordF.x / float(Size.x / ANIM0) + Time) * ANIM1;
    }

    ivec2 TexCoord = ivec2(fract(TexCoordF / vec2(Size)) * vec2(Size));

    return imageLoad(AtlasImage, Offset + TexCoord).r;
}

vec3 SRGBToLinear(vec3 Color)
{
    return mix(pow((Color + 0.055) * (1.0 / 1.055), vec3(2.4)),
        Color * (1.0 / 12.92),
        lessThanEqual(Color, vec3(0.04045)));
}

vec3 GetSkyColor()
{
    ivec4 Texture = Textures[InTextureIndex];
    ivec2 Offset = Texture.xy;
    ivec2 Size = Texture.zw;
    Size.x /= 2;
    vec2 TexCoordF = InTexCoord;
    TexCoordF /= 4;
    TexCoordF.y *= -1.0;

    TexCoordF += floor(Time * 16.0) / 2.0;
    ivec2 TexCoord0 = ivec2(fract(TexCoordF / vec2(Size)) * vec2(Size));
    uint ColorIndex0 = imageLoad(AtlasImage, Offset + TexCoord0).r;
    vec3 Color0 = SRGBToLinear(imageLoad(ColormapImage, ivec2(int(ColorIndex0), 32)).rgb);

    TexCoordF += floor(Time * 6.0) / 2.0;
    ivec2 TexCoord1 = ivec2(fract(TexCoordF / vec2(Size)) * vec2(Size));
    TexCoord1.x += Size.x;
    uint ColorIndex1 = imageLoad(AtlasImage, Offset + TexCoord1).r;
    vec3 Color1 = SRGBToLinear(imageLoad(ColormapImage, ivec2(int(ColorIndex1), 32)).rgb);

    return length(Color0) > length(Color1) ? Color0 : Color1;
}

vec3 GetLitColor(uint Color, float Light)
{
    int Shade;
    if (Light <= 1.0)
    {
        Shade = 64 - int(Light * 32.0);
    }
    else
    {
        Shade = 32 - int(clamp(Light - 1.0, 0.0, 1.0) * 32.0);
    }

    return SRGBToLinear(imageLoad(ColormapImage, ivec2(int(Color), Shade)).rgb);
}

float LightAnimationForType(int Type)
{
    if (Type == 10)
    {
        return clamp((cos(Time * 3.0) + cos(Time * 7.0) + cos(Time * 10.0)), 0.0, 1.0);
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
    SGPUSurface Surface = Surfaces[InSurfaceIndex];
    if (Surface.Water != 0 || Surface.Sky != 0)
    {
        return 1.0;
    }

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
    SGPUSurface Surface = Surfaces[InSurfaceIndex];
    if (Surface.Sky != 0)
    {
        OutColor.rgb = GetSkyColor();
    }
    else
    {
        OutColor.rgb = GetLitColor(SampleAtlasTexture(Textures[InTextureIndex], InTexCoord), SampleLightmap());
    }

    OutColor.a = 1.0;
}
