#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 16;
layout(constant_id = 1) const uint IMAGE_SIZE = 512;
layout(constant_id = 2) const uint RADIUS = 4;

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba32f) restrict readonly uniform image2D uTex0;
layout(set = 0, binding = 1, rgba32f) restrict writeonly uniform image2D uTex1;

int cKernelSize = int(RADIUS);
int cKernelHalfDist = cKernelSize / 2;
float recKernelSize = 1.0 / float(cKernelSize);

void main()
{
    int y = int(gl_GlobalInvocationID.x);

    // avoid processing pixels that are out of texture dimensions!
    if (y >= IMAGE_SIZE) return;

    vec4 colorSum = imageLoad(uTex0, ivec2(y, 0)) * float(cKernelHalfDist);
    for (int x = 0; x <= cKernelHalfDist; x++)
        colorSum += imageLoad(uTex0, ivec2(y, x));

    for (int x = 0; x < IMAGE_SIZE; x++)
    {
        imageStore(uTex1, ivec2(y, x), colorSum * recKernelSize);

        // move window to the next
        vec4 leftBorder = imageLoad(uTex0, ivec2(y, max(x - cKernelHalfDist, 0)));
        vec4 rightBorder = imageLoad(uTex0, ivec2(y, min(x + cKernelHalfDist + 1, IMAGE_SIZE - 1)));

        colorSum -= leftBorder;
        colorSum += rightBorder;
    }
}
