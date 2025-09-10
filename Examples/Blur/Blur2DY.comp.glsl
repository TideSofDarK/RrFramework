#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 32;
layout(constant_id = 1) const uint IMAGE_SIZE = 512;
layout(constant_id = 2) const uint KERNEL_SIZE = 32;

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba32f) restrict readonly uniform image2D SrcImage;
layout(set = 0, binding = 1, rgba32f) restrict writeonly uniform image2D DstImage;

void main()
{
    int Y = int(gl_GlobalInvocationID.x);

    if (Y >= IMAGE_SIZE) return;

    const int HALF_KERNEL_SIZE = int(KERNEL_SIZE) / 2;
    const float KERNEL_SIZE_RECIPROCAL = 1.0 / float(KERNEL_SIZE);

    vec4 Accumulator = imageLoad(SrcImage, ivec2(Y, 0)) * float(HALF_KERNEL_SIZE);
    for (int X = 0; X <= HALF_KERNEL_SIZE; X++)
        Accumulator += imageLoad(SrcImage, ivec2(Y, X));

    for (int X = 0; X < IMAGE_SIZE; X++)
    {
        imageStore(DstImage, ivec2(Y, X), Accumulator * KERNEL_SIZE_RECIPROCAL);

        vec4 LeftBorder = imageLoad(SrcImage, ivec2(Y, max(X - HALF_KERNEL_SIZE, 0)));
        vec4 RightBorder = imageLoad(SrcImage, ivec2(Y, min(X + HALF_KERNEL_SIZE + 1, IMAGE_SIZE - 1)));

        Accumulator -= LeftBorder;
        Accumulator += RightBorder;
    }
}
