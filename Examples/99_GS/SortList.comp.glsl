#version 460 core
#extension GL_ARB_shading_language_include : require

#include "Shared.glsl"

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) uniform UniformData
{
    mat4 View;
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

void main() {
    uint ThreadID = gl_GlobalInvocationID.x;

    Entries[ThreadID].Index = ThreadID;
    Entries[ThreadID].Z = -(View * Splats[ThreadID].Position).z;
    if (ThreadID >= Count)
    {
        Entries[ThreadID].Z = intBitsToFloat(2139095039);
    }
}
