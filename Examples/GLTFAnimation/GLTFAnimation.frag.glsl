#version 460

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in flat uint InInstanceIndex;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform UGlobals
{
    mat4 View;
    mat4 Projection;
    float Time;
};

struct SGPUModel
{
    mat4 Model;
    vec4 Color;
    ivec4 Data;
};

layout(set = 1, binding = 0) readonly buffer UModels
{
    SGPUModel Models[];
};

void main()
{
    float Dot = clamp(dot(InNormal, inverse(View)[2].xyz), 0.5, 1.0);
    OutColor = Models[InInstanceIndex].Color;
    OutColor.rgb = OutColor.rgb * Dot;
}
