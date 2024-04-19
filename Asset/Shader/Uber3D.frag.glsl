#version 460

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_UV;
layout(location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform Uber3DGlobals {
    mat4 view;
    mat4 intermediate;
    mat4 proj;
    vec4 ambientColor;
} ub_shaderGlobals;

layout(set = 0, binding = 1) uniform sampler2D u_prerenderedDepth;

//layout(std430,set = 0, binding = 2) buffer TestStorageBuffer{
//    float values[];
//} storageBuffer;

layout(set = 1, binding = 0) uniform ObjectData {
    mat4 model;
} ub_objectData;

layout(set = 1, binding = 1) uniform sampler2D u_texture;

void main()
{
    // outFragColor = vec4(mix(inColor, ub_sceneData.ambientColor.rgb, ub_sceneData.ambientColor.a), 1.0f);
    // vec3 color = vec3(1.0f);
    // out_color = vec4(color, 1.0f);
    vec3 color = texture(u_texture, vec2(in_UV.x, 1.0f - in_UV.y)).rgb;
    color *= -dot(in_normal, vec3(0.0, 0.0, -1.0));
    out_color = vec4(color, 1.0f);

    // out_color = vec4(texture(u_prerenderedDepth, in_UV).r, 0.0f, 0.0f, 1.0f);

    color = vec3(1.0f, 1.0f, 1.0f);
    color *= -dot(in_normal, vec3(0.0, 0.0, -1.0));
    out_color = vec4(color, 1.0f);
}