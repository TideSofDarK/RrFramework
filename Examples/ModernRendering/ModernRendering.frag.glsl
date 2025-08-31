#version 450

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec3 InPosition;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 View;
    mat4 Projection;
    float Time;
};

struct SGPUPointLight
{
    vec4 Position;
    vec4 Ambient;
    vec4 Diffuse;
    vec4 Specular;
    float Constant;
    float Linear;
    float Quadratic;
    float Padding;
};

layout(set = 1, binding = 0) readonly buffer SGPULights
{
    SGPUPointLight PointLights[];
};

void main()
{
    vec4 Result = vec4(vec3(0.5f), 1.0);
    for (uint Index = 0; Index < PointLights.length(); ++Index)
    {
        SGPUPointLight Light = PointLights[Index];

        float Distance = distance(Light.Position.xyz, InPosition);
        float Attenuation = 1.0 / (Light.Constant +
                                   Light.Linear * Distance +
                                   Light.Quadratic * Distance * Distance);
        Result.xyz *= Attenuation;
    }
    OutColor = Result;
}
