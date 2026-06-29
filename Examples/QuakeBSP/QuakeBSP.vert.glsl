#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in uint InSurfaceIndex;
layout(location = 2) in vec3 InNormal;
layout(location = 3) in uint InTextureIndex;
layout(location = 4) in uint InFaceIndex;

layout(location = 0) out vec3 OutNormal;
layout(location = 1) out vec4 OutTexCoord;
layout(location = 2) out flat uint OutSurfaceIndex;
layout(location = 3) out flat uint OutTextureIndex;
layout(location = 4) out flat uint OutFaceIndex;

struct SGPUSurface
{
    vec3 VectorX;
    float OffsetX;
    vec3 VectorY;
    float OffsetY;
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
layout(set = 0, binding = 4) readonly buffer UFaces
{
    SGPUFace Faces[];
};

void main()
{
    gl_Position = Projection * View * Model * vec4(InPosition, 1.0);
    OutNormal = (Model * vec4(InNormal, 0.0)).xyz;

    SGPUFace Face = Faces[InFaceIndex];

    SGPUSurface Surface = Surfaces[InSurfaceIndex];
    if (Surface.Sky != 0)
    {
        OutTexCoord = vec4(InPosition - inverse(View)[3].xyz, 0.0);
        OutTexCoord.z *= 3.0;
    }
    else
    {
        OutTexCoord.xy = vec2(
                dot(InPosition, Surface.VectorX) + Surface.OffsetX,
                dot(InPosition, Surface.VectorY) + Surface.OffsetY);
        OutTexCoord.zw = vec2(Face.LightmapSize) * 0.5 + (OutTexCoord.xy - Face.MidPolyUV) / 16.0;
    }

    OutSurfaceIndex = InSurfaceIndex;
    OutTextureIndex = InTextureIndex;
    OutFaceIndex = InFaceIndex;
}
