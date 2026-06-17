#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in float InTexCoordX;
layout(location = 2) in vec3 InNormal;
layout(location = 3) in float InTexCoordY;
layout(location = 4) in uvec4 InBoneIndices;
layout(location = 5) in vec4 InBoneWeights;

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
};

layout(set = 1, binding = 0) readonly buffer UModels
{
    SGPUModel Models[];
};

struct SGPUBone
{
    mat4 Transform;
    mat4 InverseBind;
};

layout(set = 2, binding = 0) readonly buffer UBones
{
    SGPUBone Bones[];
};

void main()
{
    vec4 Position = vec4(InPosition, 1.0);
    mat4 Model = Models[gl_InstanceIndex].Model;
    mat4 InvModel = inverse(Model);
    SGPUBone Bone0 = Bones[InBoneIndices.x];
    SGPUBone Bone1 = Bones[InBoneIndices.y];
    SGPUBone Bone2 = Bones[InBoneIndices.z];
    SGPUBone Bone3 = Bones[InBoneIndices.w];
    mat4 Skin = InBoneWeights.x * (InvModel * Bone0.Transform * Bone0.InverseBind) +
            InBoneWeights.y * (InvModel * Bone1.Transform * Bone1.InverseBind) +
            InBoneWeights.z * (InvModel * Bone2.Transform * Bone2.InverseBind) +
            InBoneWeights.w * (InvModel * Bone3.Transform * Bone3.InverseBind);
    gl_Position = Projection * View * Model * Skin * Position;
    OutNormal = mat3(transpose(inverse(Model * Skin))) * InNormal;
    OutInstanceIndex = gl_InstanceIndex;
    OutUV.x = InTexCoordX;
    OutUV.y = InTexCoordY;
}
