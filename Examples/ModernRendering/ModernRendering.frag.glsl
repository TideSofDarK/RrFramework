#version 450

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec3 InPosition;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 View;
    mat4 Projection;
    vec3 CameraPosition;
    float Time;
};

struct SGPUPointLight
{
    vec3 Position;
    float FarPlane;
    vec4 Ambient;
    vec4 Diffuse;
    vec4 Specular;
    float Constant;
    float Linear;
    float Quadratic;
    float Bias;
};

layout(set = 1, binding = 0) readonly buffer SGPULights
{
    SGPUPointLight PointLights[];
};

layout(set = 1, binding = 1) uniform samplerCube PointShadowMaps[4];

float PointPCF(
    in float CameraDistance,
    in vec3 FragToLight,
    in SGPUPointLight Light,
    in samplerCube ShadowMap)
{
    float CurrentDepth = length(FragToLight);
    float Shadow = 0.0;
    const float Samples = 4.0;
    const float Offset = 0.005;
    const float Step = Offset / (Samples * 0.5);
    for (float X = -Offset; X < Offset; X += Step)
    {
        for (float Y = -Offset; Y < Offset; Y += Step)
        {
            for (float Z = -Offset; Z < Offset; Z += Step)
            {
                float ClosestDepth = texture(ShadowMap,
                        FragToLight + vec3(X, Y, Z)).r
                        * Light.FarPlane;
                Shadow += CurrentDepth - Light.Bias > ClosestDepth ? 1.0 : 0.0;
            }
        }
    }
    return Shadow / (Samples * Samples * Samples);
}

void main()
{
    float CameraDistance = distance(CameraPosition, InPosition);
    vec4 Result = vec4(vec3(0.5f), 1.0);
    for (uint Index = 0; Index < PointLights.length(); ++Index)
    {
        SGPUPointLight Light = PointLights[Index];

        float Distance = distance(Light.Position, InPosition);
        float Attenuation = 1.0 / (Light.Constant +
                    Light.Linear * Distance +
                    Light.Quadratic * Distance * Distance);

        vec3 FragToLight = InPosition - Light.Position.xyz;
        float Shadow = PointPCF(
                CameraDistance,
                FragToLight,
                Light,
                PointShadowMaps[Index]);

        Result.xyz = vec3(0.01) + (vec3(0.3) * Attenuation) * (1.0 - Shadow);
    }
    OutColor = Result;
}
