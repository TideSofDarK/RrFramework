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
    float BleedReduction;
    vec4 Ambient;
    vec4 Diffuse;
    vec4 Specular;
    float Radius;
    float Intensity;
    float Falloff;
    float Bias;
};

layout(set = 1, binding = 0) readonly buffer SGPULights
{
    SGPUPointLight PointLights[];
};

layout(set = 1, binding = 1) uniform samplerCube PointShadowMaps[4];

float Linstep(float Min, float Max, float V)
{
    return clamp((V - Min) / (Max - Min), 0.0, 1.0);
}

float ReduceLightBleeding(float PMax, float Amount)
{
    return Linstep(Amount, 1.0, PMax);
}

float PointVSM(in vec2 Moments, in float CurrentDepth, in SGPUPointLight Light) {
    float P = step(CurrentDepth, Moments.x + Light.Bias);
    float Variance = max(Moments.y - Moments.x * Moments.x, 0.000001);
    float Dist = CurrentDepth - Moments.x;
    float PMax = Variance / (Variance + Dist * Dist);
    PMax = ReduceLightBleeding(PMax, Light.BleedReduction);

    return max(P, PMax);
}

float PointAttenuate(
    in float Distance,
    in SGPUPointLight Light)
{
    float S = Distance / Light.Radius;

    if (S >= 1.0)
        return 0.0;

    float S2 = S * S;

    return Light.Intensity * (1 - S2) * (1 - S2) / (1 + Light.Falloff * S);
}

void main()
{
    float CameraDistance = distance(CameraPosition, InPosition);
    vec4 Result = vec4(0.0, 0.0, 0.0, 1.0);
    vec3 BaseColor = (InNormal + 0.5) * 0.5;
    for (uint Index = 0; Index < PointLights.length(); ++Index)
    {
        SGPUPointLight Light = PointLights[Index];

        vec3 LightToFrag = Light.Position.xyz - InPosition;
        float LightToFragDistance = length(LightToFrag);
        if (LightToFragDistance > Light.Radius) continue;

        LightToFrag = normalize(LightToFrag);
        float Shadow = PointVSM(
                texture(PointShadowMaps[Index], -LightToFrag).rg,
                LightToFragDistance,
                Light);
        float Lambert = max(0.0, dot(InNormal, LightToFrag));
        float Attenuation = max(0.0, PointAttenuate(LightToFragDistance, Light));
        Shadow = max(0.1, Shadow);

        Result.rgb += BaseColor * (Attenuation * Lambert * Shadow);
    }
    OutColor = Result;
}
