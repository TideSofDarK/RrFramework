#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in uint InSurfaceIndex;
layout(location = 2) in vec3 InNormal;
layout(location = 3) in uint InTextureIndex;
layout(location = 4) in uint InFaceIndex;

layout(location = 0) out vec3 OutNormal;
layout(location = 1) out vec2 OutTexCoord;
layout(location = 2) out flat uint OutTextureIndex;
layout(location = 3) out flat uint OutFaceIndex;
layout(location = 4) out vec2 OutLightmapTexCoord;

struct SGPUSurface
{
    vec3 VectorX;
    float DistanceX;
    vec3 VectorY;
    float DistanceY;
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
    OutTexCoord = vec2(
            dot(InPosition, Surface.VectorX) + Surface.DistanceX,
            dot(InPosition, Surface.VectorY) + Surface.DistanceY);

    vec2 MidTex = vec2(Face.LightmapSize) * 0.5;
    OutLightmapTexCoord = MidTex + (OutTexCoord - Face.MidPolyUV) / 16.0;

    OutTextureIndex = InTextureIndex;
    OutFaceIndex = InFaceIndex;
}
