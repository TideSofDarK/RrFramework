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

layout(binding = 0, rg32f) restrict readonly uniform image2DArray image;
layout(binding = 1, rg32f) restrict writeonly uniform image2DArray image_out;

ivec3 leading_face(uint y) {
    uint face = gl_GlobalInvocationID.z;
    uint x = gl_GlobalInvocationID.x;

    switch (face) {
        case POS_X:
        return ivec3(y, IMAGE_SIZE - (x + 1), POS_Y);
        case NEG_X:
        return ivec3(IMAGE_SIZE - (y + 1), x, POS_Y);
        case POS_Y:
        return ivec3(IMAGE_SIZE - (x + 1), IMAGE_SIZE - (y + 1), NEG_Z);
        case NEG_Y:
        return ivec3(x, y, POS_Z);
        case POS_Z:
        return ivec3(x, y, POS_Y);
        default:
        return ivec3(IMAGE_SIZE - (x + 1), IMAGE_SIZE - (y + 1), POS_Y);
    }
}

ivec3 trailing_face(uint y) {
    uint face = gl_GlobalInvocationID.z;
    uint x = gl_GlobalInvocationID.x;

    switch (face) {
        case POS_X:
        return ivec3(IMAGE_SIZE - (y + 1), x, NEG_Y);
        case NEG_X:
        return ivec3(y, IMAGE_SIZE - (x + 1), NEG_Y);
        case POS_Y:
        return ivec3(x, y, POS_Z);
        case NEG_Y:
        return ivec3(IMAGE_SIZE - (x + 1), IMAGE_SIZE - (y + 1), NEG_Z);
        case POS_Z:
        return ivec3(x, y, NEG_Y);
        default:
        return ivec3(IMAGE_SIZE - (x + 1), IMAGE_SIZE - (y + 1), NEG_Y);
    }
}

void main() {
    uint face = gl_GlobalInvocationID.z;
    uint x = gl_GlobalInvocationID.x;

    vec2 accumulator = vec2(0.0);
    float per_texel = 1.0 / float((RADIUS << 1) + 1);

    for (uint y = IMAGE_SIZE - RADIUS; y < IMAGE_SIZE; y++) {
        accumulator += imageLoad(image, leading_face(y)).rg;
    }

    for (uint y = 0; y < RADIUS; y++) {
        accumulator += imageLoad(image, ivec3(x, y, face)).rg;
    }

    for (uint y = 0; y < RADIUS; y++) {
        accumulator += imageLoad(image, ivec3(x, y + RADIUS, face)).rg;
        imageStore(image_out, ivec3(x, y, face), vec4(accumulator * per_texel, 0.0, 0.0));
        accumulator -= imageLoad(image, leading_face((IMAGE_SIZE - RADIUS) + y)).rg;
    }

    for (uint y = RADIUS; y < IMAGE_SIZE - RADIUS; y++) {
        accumulator += imageLoad(image, ivec3(x, y + RADIUS, face)).rg;
        imageStore(image_out, ivec3(x, y, face), vec4(accumulator * per_texel, 0.0, 0.0));
        accumulator -= imageLoad(image, ivec3(x, y - RADIUS, face)).rg;
    }

    for (uint y = IMAGE_SIZE - RADIUS; y < IMAGE_SIZE; y++) {
        accumulator += imageLoad(image, trailing_face((y + RADIUS) - IMAGE_SIZE)).rg;
        imageStore(image_out, ivec3(x, y, face), vec4(accumulator * per_texel, 0.0, 0.0));
        accumulator -= imageLoad(image, ivec3(x, y - RADIUS, face)).rg;
    }
}
