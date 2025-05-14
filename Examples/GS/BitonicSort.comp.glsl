#version 460 core
#extension GL_ARB_shading_language_include : require

#include "Shared.glsl"

#define LOCAL_SORT 0
#define LOCAL_DISPERSE 1
#define BIG_FLIP 2
#define BIG_DISPERSE 3

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) readonly buffer SplatData
{
    SGPUSplat Splats[];
};

layout(set = 0, binding = 1) buffer Indices
{
    SSortEntry Entries[];
};

layout(set = 1, binding = 0) uniform SortInfo
{
    uint Count;
    uint Height;
    uint Algorithm;
} Info;

shared SSortEntry LocalEntries[gl_WorkGroupSize.x * 2];

bool Compare(const float LeftZ, const float RightZ) {
    return LeftZ > RightZ;
}

void GlobalCAS(uvec2 Indices) {
    if (Compare(Entries[Indices.x].Z, Entries[Indices.y].Z)) {
        SSortEntry Temp = Entries[Indices.x];
        Entries[Indices.x]= Entries[Indices.y];
        Entries[Indices.y]= Temp;
    }
}

void LocalCAS(uvec2 Indices) {
    if (Compare(LocalEntries[Indices.x].Z, LocalEntries[Indices.y].Z)) {
        SSortEntry Temp = LocalEntries[Indices.x];
        LocalEntries[Indices.x] = LocalEntries[Indices.y];
        LocalEntries[Indices.y] = Temp;
    }
}

void BigFlip(in uint Height) {
    uint GlobalThreadID = gl_GlobalInvocationID.x;
    uint Half = Height / 2;

    uint Offset = ((2 * GlobalThreadID) / Height) * Height;
    uvec2 Indices;
    Indices.x = Offset + (GlobalThreadID % Half);
    Indices.y = Offset + Height - (GlobalThreadID % Half) - 1;

    GlobalCAS(Indices);
}

void BigDisperse(in uint Height) {
    uint GlobalThreadID = gl_GlobalInvocationID.x;

    uint Half = Height / 2;

    uint Offset = ((2 * GlobalThreadID) / Height) * Height;
    uvec2 Indices;
    Indices.x = Offset + (GlobalThreadID % (Half));
    Indices.y = Offset + (GlobalThreadID % (Half)) + Half;

    GlobalCAS(Indices);
}

void LocalFlip(in uint Height) {
    uint ThreadID = gl_LocalInvocationID.x;
    barrier();

    uint Half = Height / 2;
    uvec2 Indices =
        uvec2(Height * ((2 * ThreadID) / Height)) +
            uvec2(ThreadID % Half, Height - 1 - (ThreadID % Half));

    LocalCAS(Indices);
}

void LocalDisperse(in uint Height) {
    uint ThreadID = gl_LocalInvocationID.x;
    for (; Height > 1; Height /= 2) {
        barrier();
        uint Half = Height / 2;
        uvec2 Indices =
            uvec2(Height * ((2 * ThreadID) / Height)) +
                uvec2(ThreadID % Half, Half + (ThreadID % Half));

        LocalCAS(Indices);
    }
}

void LocalSort(uint Height) {
    uint ThreadID = gl_LocalInvocationID.x;
    for (uint CurrentHeight = 2; CurrentHeight <= Height; CurrentHeight *= 2) {
        LocalFlip(CurrentHeight);
        LocalDisperse(CurrentHeight / 2);
    }
}

void main() {
    uint ThreadID = gl_LocalInvocationID.x;
    uint Offset = gl_WorkGroupSize.x * 2 * gl_WorkGroupID.x;

    if (Info.Algorithm <= LOCAL_DISPERSE) {
        LocalEntries[ThreadID * 2] = Entries[Offset + ThreadID * 2];
        LocalEntries[ThreadID * 2 + 1] = Entries[Offset + ThreadID * 2 + 1];
    }

    switch (Info.Algorithm) {
        case LOCAL_SORT:
        {
            LocalSort(Info.Height);
        }
        break;
        case LOCAL_DISPERSE:
        {
            LocalDisperse(Info.Height);
        }
        break;
        case BIG_FLIP:
        {
            BigFlip(Info.Height);
        }
        break;
        case BIG_DISPERSE:
        {
            BigDisperse(Info.Height);
        }
        break;
    }

    if (Info.Algorithm <= LOCAL_DISPERSE) {
        barrier();
        Entries[Offset + ThreadID * 2] = LocalEntries[ThreadID * 2];
        Entries[Offset + ThreadID * 2 + 1] = LocalEntries[ThreadID * 2 + 1];
    }
}
