#version 450

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec3 InPosition;
layout(location = 3) in vec3 InNormalVS;

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

layout(set = 1, binding = 1) uniform samplerCubeShadow PointShadowMaps[4];

float PointPCF(
    in float CameraDistance,
    in vec3 FragToLight,
    in SGPUPointLight Light,
    in samplerCubeShadow ShadowMap)
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
                Shadow += texture(
                        ShadowMap,
                        vec4(FragToLight + vec3(X, Y, Z), (CurrentDepth / 100.0) - Light.Bias));
            }
        }
    }
    return Shadow / (Samples * Samples * Samples);
}

void main()
{
    float CameraDistance = distance(CameraPosition, InPosition);
    vec4 Result = vec4((InNormalVS + 0.5) * 0.5, 1.0);
    for (uint Index = 0; Index < PointLights.length(); ++Index)
    {
        SGPUPointLight Light = PointLights[Index];

        float Distance = distance(Light.Position, InPosition);
        float Attenuation = 1.0 / (Light.Constant +
                    Light.Linear * Distance +
                    Light.Quadratic * Distance * Distance);

        vec3 FragToLight = InPosition - Light.Position.xyz;
        float CurrentDepth = length(FragToLight);
        float Shadow = PointPCF(
                CameraDistance,
                FragToLight,
                Light,
                PointShadowMaps[Index]);

        // Shadow *= max(0.0, -dot(InNormal, FragToLight));

        Result.rgb = Result.rgb * 0.2 + (Result.rgb * 0.8 * Attenuation * Shadow);
    }
    OutColor = Result;
}
