#version 450

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_UV;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform ShaderGlobals {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
} ub_shaderGlobals;

layout(set = 0, binding = 1) uniform sampler2D u_testSampler;

layout(set = 1, binding = 0) uniform ObjectData {
    mat4 model;
} ub_objectData;

void main()
{
    // outFragColor = vec4(mix(inColor, ub_sceneData.ambientColor.rgb, ub_sceneData.ambientColor.a), 1.0f);
    vec3 color = vec3(1.0f);
    out_color = vec4(color, 1.0f);
}
