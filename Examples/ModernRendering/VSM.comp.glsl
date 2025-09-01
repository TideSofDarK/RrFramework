#version 460 core

layout(constant_id = 0) const uint LocalX = 1;
layout(constant_id = 1) const uint LocalY = 1;

layout(local_size_x_id = 0, local_size_y_id = 1) in;

layout(set = 0, binding = 0, rg32f) writeonly uniform image2DArray DstImage;
layout(set = 0, binding = 1, rg32f) readonly uniform image2DArray SrcImage;

void main() {
    // ivec2 Size = imageSize(DstImage);
    // ivec2 Coord = ivec2(gl_GlobalInvocationID.xy);
    // if (Coord.x < Size.x && Coord.y < Size.y)
    // {
    //     float Depth = texelFetch(SrcImage, Coord, 0).r;
    //     imageStore(DstImage, Coord, vec4(Depth, Depth * Depth, 0.0, 0.0));
    // }
}
