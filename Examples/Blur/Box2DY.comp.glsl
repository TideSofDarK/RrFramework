#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 32;
layout(constant_id = 1) const uint KERNEL_SIZE = 5;

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba32f) restrict readonly uniform image2D SrcImage;
layout(set = 0, binding = 1, rgba32f) restrict writeonly uniform image2D DstImage;
layout(set = 0, binding = 2) uniform SGPUUniform {
    ivec2 ImageSize;
};

void main()
{
    int X = int(gl_GlobalInvocationID.x);

    if (X >= ImageSize.x) return;

    const int HALF_KERNEL_SIZE = int(KERNEL_SIZE) / 2;
    const float KERNEL_SIZE_RECIPROCAL = 1.0 / float(KERNEL_SIZE);

    vec4 Accumulator = imageLoad(SrcImage, ivec2(X, 0)) * float(HALF_KERNEL_SIZE);
    for (int Y = 0; Y <= HALF_KERNEL_SIZE; Y++)
        Accumulator += imageLoad(SrcImage, ivec2(X, Y));

    for (int Y = 0; Y < ImageSize.y; Y++)
    {
        imageStore(DstImage, ivec2(X, Y), Accumulator * KERNEL_SIZE_RECIPROCAL);

        vec4 LeftBorder = imageLoad(SrcImage, ivec2(X, max(Y - HALF_KERNEL_SIZE, 0)));
        vec4 RightBorder = imageLoad(SrcImage, ivec2(X, min(Y + HALF_KERNEL_SIZE + 1, ImageSize.y - 1)));

        Accumulator -= LeftBorder;
        Accumulator += RightBorder;
    }
}
