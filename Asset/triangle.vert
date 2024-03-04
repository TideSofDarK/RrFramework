#version 450
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

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

layout(set = 0, binding = 0) readonly buffer SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
} ub_sceneData;

layout(push_constant) uniform constants
{
    mat4 viewProjection;
    VertexBuffer vertexBuffer;
} PushConstants;

void main()
{
    //load vertex data from device adress
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    //output data
    gl_Position = PushConstants.viewProjection * vec4(v.position, 1.0f);
    // outColor = v.color.xyz;
    outColor = v.normal;
    // outColor = vec3(v.uv_x, v.uv_y, 1.0f);
    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
}
