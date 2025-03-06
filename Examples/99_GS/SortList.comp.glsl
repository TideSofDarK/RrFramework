#version 460 core
#extension GL_ARB_shading_language_include : require

#include "Shared.glsl"

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) uniform UniformData
{
    mat4 ViewProjection;
    uint Count;
};

layout(set = 0, binding = 1) readonly buffer SplatData
{
    SGPUSplat Splats[];
};

layout(set = 0, binding = 2) buffer OutputSortList
{
    SSortEntry Entries[];
};

layout(set = 0, binding = 3) buffer OutputIndirectCommand
{
    uint VertexCount;
    uint InstanceCount;
    uint FirstVertex;
    uint FirstInstance;
} IndirectCommand;

shared uint LocalCount;

void main() {
    uint ThreadID = gl_GlobalInvocationID.x;

    if (ThreadID >= Count)
    {
        Entries[ThreadID].Z = intBitsToFloat(2139095039);
        return;
    }

    if (gl_LocalInvocationID.x == 0)
    {
        LocalCount = 0;
    }

    vec4 SplatPosition = Splats[ThreadID].Position;
    vec4 Projected = ViewProjection * SplatPosition;
    float Z = Projected.z;
    Entries[ThreadID].Index = ThreadID;
    Entries[ThreadID].Z = -Z;

    barrier();
    atomicAdd(LocalCount, int(IsPointInFrustum(Projected.xyz, ViewProjection)));
    barrier();

    if (gl_LocalInvocationID.x == 0)
    {
        if (LocalCount > 0)
        {
            atomicAdd(IndirectCommand.InstanceCount, gl_WorkGroupSize.x);
            atomicMin(IndirectCommand.InstanceCount, Count);
        }
    }
}
