#extension GL_EXT_buffer_reference : require

#define RR_UBER3D_SET_GLOBALS 0

struct Vertex {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};
