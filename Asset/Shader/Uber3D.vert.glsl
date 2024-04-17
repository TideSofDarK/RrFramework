#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_UV;
layout(location = 2) out vec3 out_normal;

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

struct SUber3DObject {
    mat4 model;
    VertexBuffer vertexBuffer;
    float paddingA;
};

layout(buffer_reference, std430) readonly buffer PerObjectBuffer {
    SUber3DObject objects[];
};

layout(set = 0, binding = 0) uniform Uber3DGlobals {
    mat4 view;
    mat4 intermediate;
    mat4 proj;
    vec4 ambientColor;
} ub_shaderGlobals;

layout(set = 1, binding = 0) uniform Uber3DObject {
    PerObjectBuffer perObjectBuffer;
} ub_perObjectBuffer;

layout(push_constant) uniform Uber3DPushConstants
{
    PerObjectBuffer perObjectBuffer;
    uint ObjectIndex;
} ub_constants;

void main()
{
    SUber3DObject object = ub_constants.perObjectBuffer.objects[ub_constants.ObjectIndex];
    Vertex v = object.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = ub_shaderGlobals.proj * ub_shaderGlobals.intermediate * ub_shaderGlobals.view * object.model * vec4(v.position, 1.0f);

    out_color = v.color;
    out_UV.x = v.uv_x;
    out_UV.y = v.uv_y;
    out_normal = (ub_shaderGlobals.view * object.model * vec4(v.normal, 0.0f)).rgb;
}
