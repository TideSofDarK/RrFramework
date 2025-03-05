#include "Shared.glsl"

layout(set = 0, binding = 0) uniform Globals
{
    mat4 View;
    mat4 Projection;
    vec3 HFOVFocal;
};

layout(set = 0, binding = 1) readonly buffer GPUSplats
{
    SGPUSplat Splats[];
};

layout(set = 0, binding = 2) readonly buffer SortedIndices
{
    SSortEntry Entries[];
};
