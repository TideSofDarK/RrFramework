#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_UV;

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

layout(set = 0, binding = 0) uniform ShaderGlobals {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
} ub_shaderGlobals;

layout(set = 1, binding = 0) uniform ObjectData {
    mat4 model;
    VertexBuffer vertexBuffer;
} ub_objectData;

// layout(push_constant) uniform PushConstants
// {
//     VertexBuffer vertexBuffer;
// } ub_constants;

void main()
{
    Vertex v = ub_objectData.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = ub_shaderGlobals.viewProj * ub_objectData.model * vec4(v.position, 1.0f);
    out_color = v.color;
    out_UV.x = v.uv_x;
    out_UV.y = v.uv_y;
}
