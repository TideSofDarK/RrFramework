#version 460 core

// Contrived example of various binding techniques.

layout(local_size_x_id = 0, local_size_y_id = 0) in;

struct SS0B0Element
{
    vec4 ZeroVector;
    vec4 OneVector;
};

layout(set = 0, binding = 0) readonly buffer LS0B0
{
    SS0B0Element Element;
} S0B0;

struct SS1B1Element
{
    vec4 TwoVector;
    uint ThousandU32;
};

layout(set = 1, binding = 1) uniform LS1B1
{
    SS1B1Element Elements[]; // 8
} S1B1;

struct SS2Element
{
    vec4 Vector;
    uint U32;
};

layout(set = 2, binding = 2) buffer LS2B2
{
    SS2Element Elements[]; // 8
} S2B2;

struct SS2B4Element
{
    vec4 Vector;
    uint U32;
};

layout(set = 2, binding = 4) uniform LS2B4
{
    SS2B4Element Element;
} S2B4[4];

layout(rgba8, set = 3, binding = 13) uniform image2D StorageImage;

void main() {
    ivec2 Size = imageSize(StorageImage);
    ivec2 Coord = ivec2(gl_GlobalInvocationID.xy);
    if (Coord.x < Size.x && Coord.y < Size.y)
    {
        vec4 Color = vec4(0.0, 0.0, 0.0, 1.0);

        if (gl_LocalInvocationID.x != 0 && gl_LocalInvocationID.y != 0)
        {
            Color.r = 1.0 - (float(Coord.x) / (Size.x));
            Color.g = 1.0 - (float(Coord.y) / (Size.y));
        }

        Color.b = S2B4[1].Element.Vector.x;

        imageStore(StorageImage, Coord, Color);
    }
}
