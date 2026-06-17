#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in uvec4 InSkinIndices;
layout(location = 3) in vec4 InSkinWeights;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out uint OutInstanceIndex;

layout(set = 0, binding = 0) uniform UGlobals
{
    mat4 View;
    mat4 Projection;
    float Time;
};

struct SGPUModel
{
    mat4 Model;
    vec4 Color;
    ivec4 Data;
};

layout(set = 1, binding = 0) readonly buffer UModels
{
    SGPUModel Models[];
};

layout(set = 2, binding = 0) readonly buffer UBones
{
    mat4 Bones[];
};

void main()
{
    vec4 Position = vec4(InPosition, 1.0);
    mat4 Model = Models[gl_InstanceIndex].Model;
    int Offset = Models[gl_InstanceIndex].Data.x * 64;
    mat4 Skin = InSkinWeights.x * Bones[InSkinIndices.x + Offset] +
            InSkinWeights.y * Bones[InSkinIndices.y + Offset] +
            InSkinWeights.z * Bones[InSkinIndices.z + Offset] +
            InSkinWeights.w * Bones[InSkinIndices.w + Offset];
    gl_Position = Projection * View * Model * Skin * Position;
    OutNormal = mat3(transpose(inverse(Model * Skin))) * InNormal;
    OutInstanceIndex = gl_InstanceIndex;
}
