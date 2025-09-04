#version 450 core

#define POS_X 0
#define NEG_X 1
#define POS_Y 2
#define NEG_Y 3
#define POS_Z 4
#define NEG_Z 5

layout(constant_id = 0) const uint LOCAL_SIZE = 16;
layout(constant_id = 1) const uint IMAGE_SIZE = 512;
layout(constant_id = 2) const uint RADIUS = 4;

layout(local_size_x_id = 0, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0, rg32f) restrict readonly uniform image2DArray SrcImage;
layout(binding = 1, rg32f) restrict writeonly uniform image2DArray DstImage;

ivec3 LeadingFace(uint Y) {
    uint Face = gl_GlobalInvocationID.z;
    uint X = gl_GlobalInvocationID.x;

    switch (Face) {
        case POS_X:
        return ivec3(Y, IMAGE_SIZE - (X + 1), POS_Y);
        case NEG_X:
        return ivec3(IMAGE_SIZE - (Y + 1), X, POS_Y);
        case POS_Y:
        return ivec3(IMAGE_SIZE - (X + 1), IMAGE_SIZE - (Y + 1), NEG_Z);
        case NEG_Y:
        return ivec3(X, Y, POS_Z);
        case POS_Z:
        return ivec3(X, Y, POS_Y);
        default:
        return ivec3(IMAGE_SIZE - (X + 1), IMAGE_SIZE - (Y + 1), POS_Y);
    }
}

ivec3 TrailingFace(uint Y) {
    uint Face = gl_GlobalInvocationID.z;
    uint X = gl_GlobalInvocationID.x;

    switch (Face) {
        case POS_X:
        return ivec3(IMAGE_SIZE - (Y + 1), X, NEG_Y);
        case NEG_X:
        return ivec3(Y, IMAGE_SIZE - (X + 1), NEG_Y);
        case POS_Y:
        return ivec3(X, Y, POS_Z);
        case NEG_Y:
        return ivec3(IMAGE_SIZE - (X + 1), IMAGE_SIZE - (Y + 1), NEG_Z);
        case POS_Z:
        return ivec3(X, Y, NEG_Y);
        default:
        return ivec3(IMAGE_SIZE - (X + 1), IMAGE_SIZE - (Y + 1), NEG_Y);
    }
}

void main() {
    uint Face = gl_GlobalInvocationID.z;
    uint X = gl_GlobalInvocationID.x;

    vec2 Accumulator = vec2(0.0);
    float PerTexel = 1.0 / float((RADIUS << 1) + 1);

    for (uint Y = IMAGE_SIZE - RADIUS; Y < IMAGE_SIZE; Y++) {
        Accumulator += imageLoad(SrcImage, LeadingFace(Y)).rg;
    }

    for (uint Y = 0; Y < RADIUS; Y++) {
        Accumulator += imageLoad(SrcImage, ivec3(X, Y, Face)).rg;
    }

    for (uint Y = 0; Y < RADIUS; Y++) {
        Accumulator += imageLoad(SrcImage, ivec3(X, Y + RADIUS, Face)).rg;
        imageStore(DstImage, ivec3(X, Y, Face), vec4(Accumulator * PerTexel, 0.0, 0.0));
        Accumulator -= imageLoad(SrcImage, LeadingFace((IMAGE_SIZE - RADIUS) + Y)).rg;
    }

    for (uint Y = RADIUS; Y < IMAGE_SIZE - RADIUS; Y++) {
        Accumulator += imageLoad(SrcImage, ivec3(X, Y + RADIUS, Face)).rg;
        imageStore(DstImage, ivec3(X, Y, Face), vec4(Accumulator * PerTexel, 0.0, 0.0));
        Accumulator -= imageLoad(SrcImage, ivec3(X, Y - RADIUS, Face)).rg;
    }

    for (uint Y = IMAGE_SIZE - RADIUS; Y < IMAGE_SIZE; Y++) {
        Accumulator += imageLoad(SrcImage, TrailingFace((Y + RADIUS) - IMAGE_SIZE)).rg;
        imageStore(DstImage, ivec3(X, Y, Face), vec4(Accumulator * PerTexel, 0.0, 0.0));
        Accumulator -= imageLoad(SrcImage, ivec3(X, Y - RADIUS, Face)).rg;
    }
}
