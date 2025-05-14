#version 460 core

layout(local_size_x_id = 0) in;

layout(set = 0, binding = 0) readonly buffer Sorted
{
    uint SortedNumbers[];
};

layout(set = 0, binding = 1) readonly buffer Unsorted
{
    uint UnsortedNumbers[];
};

layout(set = 0, binding = 2, r32ui) uniform uimage2D ResultImage;

void main() {
    uint ThreadID = gl_GlobalInvocationID.x;

    uint NotEquals = uint(SortedNumbers[ThreadID] != UnsortedNumbers[ThreadID]);
    uint Equals = uint(SortedNumbers[ThreadID] == UnsortedNumbers[ThreadID]);
    imageAtomicOr(ResultImage, ivec2(0, 0), (255 * NotEquals));
    imageAtomicCompSwap(ResultImage, ivec2(0, 0), 0, (255 * Equals) << 8);
}
