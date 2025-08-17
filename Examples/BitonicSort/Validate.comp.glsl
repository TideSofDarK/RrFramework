#version 460 core

layout(local_size_x_id = 0, local_size_y_id = 1) in;
layout(constant_id = 2) const int IMAGE_SIDE = 0;

layout(set = 0, binding = 0) readonly buffer Sorted
{
    uint SortedNumbers[];
};

layout(set = 0, binding = 1) readonly buffer Unsorted
{
    uint UnsortedNumbers[];
};

layout(rgba8, set = 0, binding = 2) uniform image2D ResultImage;

void main() {
    ivec2 Coord = ivec2(gl_GlobalInvocationID.xy);
    if (Coord.x < IMAGE_SIDE && Coord.y < IMAGE_SIDE)
    {
        uint ThreadID = Coord.y * IMAGE_SIDE + Coord.x;

        uint NotEquals = uint(SortedNumbers[ThreadID] != UnsortedNumbers[ThreadID]);
        uint Equals = uint(SortedNumbers[ThreadID] == UnsortedNumbers[ThreadID]);
        imageStore(ResultImage, Coord, vec4(float(NotEquals), float(Equals), float(Coord.x < 128), 1.0));
    }
}
