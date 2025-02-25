#version 450

struct SGPUSplat
{
    vec4 Position;
    vec4 Quat;
    vec4 Scale;
    vec4 Color;
};

layout (local_size_x = 1024) in;

layout(set = 0, binding = 0) uniform Globals
{
    vec4 CameraWorldPos;
};

layout(set = 0, binding = 1) readonly buffer GPUSplats
{
    SGPUSplat Splats[];
};

layout(set = 0, binding = 2) writeonly buffer SortedIndices
{
    uint Indices[];
};

void main()
{
    /* @TODO: Sorting. */
    Indices[gl_GlobalInvocationID.x + gl_LocalInvocationID.x] = gl_GlobalInvocationID.x + gl_LocalInvocationID.x;
}
