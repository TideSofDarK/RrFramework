#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 ambientColor;
} ub_sceneData;

void main()
{
    outFragColor = vec4(mix(inColor, ub_sceneData.ambientColor.rgb, ub_sceneData.ambientColor.a), 1.0f);
    // vec3 color = vec3(0.5f);
    // outFragColor = vec4(color, 0.1f);
}
