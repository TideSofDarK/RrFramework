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

layout(local_size_x = 1, local_size_y_id = 0, local_size_z = 1) in;

layout(set = 0, binding = 0, rg32f) restrict readonly uniform image2DArray image;
layout(set = 0, binding = 1, rg32f) restrict writeonly uniform image2DArray image_out;

ivec3 leading_face(uint x) {
    uint face = gl_GlobalInvocationID.z;
    uint y = gl_GlobalInvocationID.y;

    switch (face) {
        case POS_X:
        return ivec3(x, y, POS_Z);
        case NEG_X:
        return ivec3(x, y, NEG_Z);
        case POS_Y:
        return ivec3(y, IMAGE_SIZE - (x + 1), NEG_X);
        case NEG_Y:
        return ivec3(IMAGE_SIZE - (y + 1), x, NEG_X);
        case POS_Z:
        return ivec3(x, y, NEG_X);
        default:
        return ivec3(x, y, POS_X);
    }
}

ivec3 trailing_face(uint x) {
    uint face = gl_GlobalInvocationID.z;
    uint y = gl_GlobalInvocationID.y;

    switch (face) {
        case POS_X:
        return ivec3(x, y, NEG_Z);
        case NEG_X:
        return ivec3(x, y, POS_Z);
        case POS_Y:
        return ivec3(IMAGE_SIZE - (y + 1), x, POS_X);
        case NEG_Y:
        return ivec3(y, IMAGE_SIZE - (x + 1), POS_X);
        case POS_Z:
        return ivec3(x, y, POS_X);
        default:
        return ivec3(x, y, NEG_X);
    }
}

void main() {
    uint face = gl_GlobalInvocationID.z;
    uint y = gl_GlobalInvocationID.y;

    vec2 accumulator = vec2(0.0);
    float per_texel = 1.0 / float((RADIUS << 1) + 1);

    for (uint x = IMAGE_SIZE - RADIUS; x < IMAGE_SIZE; x++) {
        accumulator += imageLoad(image, leading_face(x)).rg;
    }

    for (uint x = 0; x < RADIUS; x++) {
        accumulator += imageLoad(image, ivec3(x, y, face)).rg;
    }

    for (uint x = 0; x < RADIUS; x++) {
        accumulator += imageLoad(image, ivec3(x + RADIUS, y, face)).rg;
        imageStore(image_out, ivec3(x, y, face), vec4(accumulator * per_texel, 0.0, 0.0));
        accumulator -= imageLoad(image, leading_face((IMAGE_SIZE - RADIUS) + x)).rg;
    }

    for (uint x = RADIUS; x < IMAGE_SIZE - RADIUS; x++) {
        accumulator += imageLoad(image, ivec3(x + RADIUS, y, face)).rg;
        imageStore(image_out, ivec3(x, y, face), vec4(accumulator * per_texel, 0.0, 0.0));
        accumulator -= imageLoad(image, ivec3(x - RADIUS, y, face)).rg;
    }

    for (uint x = IMAGE_SIZE - RADIUS; x < IMAGE_SIZE; x++) {
        accumulator += imageLoad(image, trailing_face((x + RADIUS) - IMAGE_SIZE)).rg;
        imageStore(image_out, ivec3(x, y, face), vec4(accumulator * per_texel, 0.0, 0.0));
        accumulator -= imageLoad(image, ivec3(x - RADIUS, y, face)).rg;
    }
}
