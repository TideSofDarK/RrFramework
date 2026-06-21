#version 460

#define POS_X 0
#define NEG_X 1
#define POS_Y 2
#define NEG_Y 3
#define POS_Z 4
#define NEG_Z 5

layout(constant_id = 0) const uint LOCAL_SIZE = 16;
layout(constant_id = 1) const uint IMAGE_SIZE = 512;
layout(constant_id = 2) const uint RADIUS = 4;

layout(local_size_x = 1, local_size_y_id = 0, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba8) restrict readonly uniform image2DArray SrcImage;
layout(set = 0, binding = 1, rgba8) restrict writeonly uniform image2DArray DstImage;

ivec3 LeadingFace(uint X) {
    uint Face = gl_GlobalInvocationID.z;
    uint Y = gl_GlobalInvocationID.y;

    switch (Face) {
        case POS_X:
        return ivec3(X, Y, POS_Z);
        case NEG_X:
        return ivec3(X, Y, NEG_Z);
        case POS_Y:
        return ivec3(Y, IMAGE_SIZE - (X + 1), NEG_X);
        case NEG_Y:
        return ivec3(IMAGE_SIZE - (Y + 1), X, NEG_X);
        case POS_Z:
        return ivec3(X, Y, NEG_X);
        default:
        return ivec3(X, Y, POS_X);
    }
}

ivec3 TrailingFace(uint X) {
    uint Face = gl_GlobalInvocationID.z;
    uint Y = gl_GlobalInvocationID.y;

    switch (Face) {
        case POS_X:
        return ivec3(X, Y, NEG_Z);
        case NEG_X:
        return ivec3(X, Y, POS_Z);
        case POS_Y:
        return ivec3(IMAGE_SIZE - (Y + 1), X, POS_X);
        case NEG_Y:
        return ivec3(Y, IMAGE_SIZE - (X + 1), POS_X);
        case POS_Z:
        return ivec3(X, Y, POS_X);
        default:
        return ivec3(X, Y, NEG_X);
    }
}

void main() {
    uint Face = gl_GlobalInvocationID.z;
    uint Y = gl_GlobalInvocationID.y;

    vec4 Accumulator = vec4(0.0);
    float PerTexel = 1.0 / float((RADIUS << 1) + 1);

    for (uint X = IMAGE_SIZE - RADIUS; X < IMAGE_SIZE; X++) {
        Accumulator += imageLoad(SrcImage, LeadingFace(X));
    }

    for (uint X = 0; X < RADIUS; X++) {
        Accumulator += imageLoad(SrcImage, ivec3(X, Y, Face));
    }

    for (uint X = 0; X < RADIUS; X++) {
        Accumulator += imageLoad(SrcImage, ivec3(X + RADIUS, Y, Face));
        imageStore(DstImage, ivec3(X, Y, Face), Accumulator * PerTexel);
        Accumulator -= imageLoad(SrcImage, LeadingFace((IMAGE_SIZE - RADIUS) + X));
    }

    for (uint X = RADIUS; X < IMAGE_SIZE - RADIUS; X++) {
        Accumulator += imageLoad(SrcImage, ivec3(X + RADIUS, Y, Face));
        imageStore(DstImage, ivec3(X, Y, Face), Accumulator * PerTexel);
        Accumulator -= imageLoad(SrcImage, ivec3(X - RADIUS, Y, Face));
    }

    for (uint X = IMAGE_SIZE - RADIUS; X < IMAGE_SIZE; X++) {
        Accumulator += imageLoad(SrcImage, TrailingFace((X + RADIUS) - IMAGE_SIZE));
        imageStore(DstImage, ivec3(X, Y, Face), Accumulator * PerTexel);
        Accumulator -= imageLoad(SrcImage, ivec3(X - RADIUS, Y, Face));
    }
}
