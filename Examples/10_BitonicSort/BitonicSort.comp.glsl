#version 460 core

#define LOCAL_SORT 0
#define LOCAL_DISPERSE 1
#define BIG_FLIP 2
#define BIG_DISPERSE 3

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) buffer SortData
{
    uint Values[];
};

layout(set = 0, binding = 1) uniform SortInfo
{
    uint Count;
    uint Height;
    uint Algorithm;
} Info;

shared uint LocalValues[gl_WorkGroupSize.x * 2];

bool Compare(const uint Left, const uint Right) {
    return Left > Right;
}

void GlobalCAS(uvec2 idx) {
    if (Compare(Values[idx.x], Values[idx.y])) {
        uint Temp = Values[idx.x];
        Values[idx.x] = Values[idx.y];
        Values[idx.y] = Temp;
    }
}

void LocalCAS(uvec2 idx) {
    if (Compare(LocalValues[idx.x], LocalValues[idx.y])) {
        uint Temp = LocalValues[idx.x];
        LocalValues[idx.x] = LocalValues[idx.y];
        LocalValues[idx.y] = Temp;
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
        LocalValues[ThreadID * 2] = Values[Offset + ThreadID * 2];
        LocalValues[ThreadID * 2 + 1] = Values[Offset + ThreadID * 2 + 1];
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
        Values[Offset + ThreadID * 2] = LocalValues[ThreadID * 2];
        Values[Offset + ThreadID * 2 + 1] = LocalValues[ThreadID * 2 + 1];
    }
}
