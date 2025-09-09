#version 450

#define PI 3.1415926
#define DEG_TO_RAD (PI / 180.0)
#define SAMPLES_COUNT 32
#define SAMPLES_COUNT_RECIPROCAL (1.0 / float(SAMPLES_COUNT))

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
    float Energy;
    vec3 Color;
    float Specular;
    float Radius;
    float Intensity;
    float Falloff;
    float ConstantBias;
    float SlopeBias;
    float NormalBias;
    float LightSize;
    float TexelSize;
};

struct SGPUSpotLight
{
    mat4 Transform;
    mat4 View;
    mat4 Projection;
    vec3 Color;
    float Energy;
    vec3 Padding;
    float Specular;
    float Intensity;
    float InnerCone;
    float OuterCone;
    float ConstantBias;
    float SlopeBias;
    float NormalBias;
    float LightSize;
    float TexelSize;
};

layout(set = 1, binding = 0) readonly buffer SGPUPointLights
{
    SGPUPointLight PointLights[];
};

layout(set = 1, binding = 1) uniform textureCube PointShadowMaps[4];

layout(set = 1, binding = 2) readonly buffer SGPUSpotLights
{
    SGPUSpotLight SpotLights[];
};

layout(set = 1, binding = 3) uniform texture2D SpotShadowMaps[4];

layout(set = 1, binding = 4) uniform sampler RegularSampler;
layout(set = 1, binding = 5) uniform sampler ShadowSampler;

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

float SpotAttenuate(in vec3 LightToFrag, in SGPUSpotLight Light)
{
    float Dot = dot(LightToFrag, Light.Transform[2].xyz);
    float OuterConeCos = cos(Light.OuterCone * DEG_TO_RAD * 0.5);
    float InnerConeCos = cos(Light.InnerCone * DEG_TO_RAD * 0.5);
    float ActualCos = clamp(Dot, OuterConeCos, InnerConeCos);
    return smoothstep(OuterConeCos, InnerConeCos, ActualCos);
}

float VectorToDepthValue(vec3 Vec)
{
    vec3 AbsVec = abs(Vec);
    float LocalZcomp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));

    const float f = 100.0;
    const float n = 0.1;
    float NormZComp = (f + n) / (f - n) - (2 * f * n) / (f - n) / LocalZcomp;
    return (NormZComp + 1.0) * 0.5;
}

const vec2 POISSON16[] = vec2[16](
        vec2(-0.9420162, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.0941841, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );

const vec2 POISSON32[] = vec2[32](
        vec2(0.06407013, 0.05409927),
        vec2(0.7366577, 0.5789394),
        vec2(-0.6270542, -0.5320278),
        vec2(-0.4096107, 0.8411095),
        vec2(0.6849564, -0.4990818),
        vec2(-0.874181, -0.04579735),
        vec2(0.9989998, 0.0009880066),
        vec2(-0.004920578, -0.9151649),
        vec2(0.1805763, 0.9747483),
        vec2(-0.2138451, 0.2635818),
        vec2(0.109845, 0.3884785),
        vec2(0.06876755, -0.3581074),
        vec2(0.374073, -0.7661266),
        vec2(0.3079132, -0.1216763),
        vec2(-0.3794335, -0.8271583),
        vec2(-0.203878, -0.07715034),
        vec2(0.5912697, 0.1469799),
        vec2(-0.88069, 0.3031784),
        vec2(0.5040108, 0.8283722),
        vec2(-0.5844124, 0.5494877),
        vec2(0.6017799, -0.1726654),
        vec2(-0.5554981, 0.1559997),
        vec2(-0.3016369, -0.3900928),
        vec2(-0.5550632, -0.1723762),
        vec2(0.925029, 0.2995041),
        vec2(-0.2473137, 0.5538505),
        vec2(0.9183037, -0.2862392),
        vec2(0.2469421, 0.6718712),
        vec2(0.3916397, -0.4328209),
        vec2(-0.03576927, -0.6220032),
        vec2(-0.04661255, 0.7995201),
        vec2(0.4402924, 0.3640312)
    );

vec2 Rotate2D(vec2 Vec, float Cos, float Sin)
{
    return vec2(Vec.x * Cos - Vec.y * Sin, Vec.x * Sin + Vec.y * Cos);
}

float Hash12(vec2 Seed)
{
    vec3 Seed3 = fract(vec3(Seed.xyx) * .1031);
    Seed3 += dot(Seed3, Seed3.yzx + 33.33);
    return fract((Seed3.x + Seed3.y) * Seed3.z);
}

vec2 GetBiasOffsets(in float NDotL)
{
    float OffsetScaleN = sqrt(1.0 - NDotL * NDotL);
    return vec2(OffsetScaleN, min(2, OffsetScaleN / NDotL));
}

float PointPCSS(
    in textureCube ShadowMap,
    in SGPUPointLight Light,
    in vec3 FragPosition,
    in vec3 FragNormal)
{
    vec3 FragToLight = Light.Position - InPosition;
    vec3 FragToLightNormalized = normalize(FragToLight);
    float NDotL = clamp(dot(FragNormal, FragToLightNormalized), 0.0, 1.0);

    vec2 BiasOffsets = GetBiasOffsets(NDotL);
    vec3 NormalOffsetBias = FragNormal * Light.NormalBias * BiasOffsets.x;
    float Bias = Light.ConstantBias * Light.TexelSize * (Light.ConstantBias + Light.SlopeBias * BiasOffsets.y);

    vec3 SampleDir = normalize(-FragToLightNormalized + NormalOffsetBias);
    float Receiver = VectorToDepthValue(FragToLight);
    float ReceiverBiased = Receiver - Bias;

    float AdaptiveRadius = Light.LightSize * sqrt(Receiver);

    vec3 Up = abs(SampleDir.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    vec3 Tangent = normalize(cross(Up, SampleDir));
    vec3 Bitangent = cross(SampleDir, Tangent);

    float RotationAngle = Hash12(gl_FragCoord.xy) * 2.0 * PI;
    float RCos = cos(RotationAngle);
    float RSin = sin(RotationAngle);

    float Shadow = 0.0;

    const int SHADOW_SAMPLES = 32;

    for (int Index = 0; Index < SHADOW_SAMPLES; ++Index)
    {
        vec2 RotatedOffset = Rotate2D(POISSON32[Index], RCos, RSin);
        vec3 SampleDirOffset = normalize(SampleDir + (Tangent * RotatedOffset.x + Bitangent * RotatedOffset.y) * AdaptiveRadius);
        Shadow += texture(samplerCubeShadow(ShadowMap, ShadowSampler), vec4(SampleDirOffset, ReceiverBiased));
    }

    return Shadow / float(SHADOW_SAMPLES);
}

float SpotPCSS(in texture2D ShadowMap, in SGPUSpotLight Light, in vec3 FragPosition, in vec3 FragNormal)
{
    float NDotL = clamp(dot(FragNormal, -Light.Transform[2].xyz), 0.0, 1.0);

    vec2 BiasOffsets = GetBiasOffsets(NDotL);
    vec3 NormalOffsetBias = FragNormal * Light.NormalBias * BiasOffsets.x;

    vec4 FragLS = Light.Projection * Light.View * vec4(FragPosition + NormalOffsetBias, 1.0);
    float Receiver = FragLS.z / FragLS.w;
    if (Receiver < 0.0 || 1.0 < Receiver)
    {
        return 1.0;
    }
    vec2 ShadowCoords = (FragLS.xy / FragLS.w) * 0.5 + 0.5;

    float Bias = Light.ConstantBias * Light.TexelSize * (Light.ConstantBias + Light.SlopeBias * BiasOffsets.y);
    float ReceiverBiased = Receiver - Bias;

    float AdaptiveRadius = Light.LightSize * sqrt(Receiver);

    float RotationAngle = Hash12(gl_FragCoord.xy) * 2.0 * PI;
    float RCos = cos(RotationAngle);
    float RSin = sin(RotationAngle);

    float Shadow = 0.0;

    const int SHADOW_SAMPLES = 32;

    for (int Index = 0; Index < SHADOW_SAMPLES; ++Index)
    {
        vec2 Offset = Rotate2D(POISSON32[Index], RCos, RSin) * AdaptiveRadius;
        Shadow += texture(sampler2DShadow(ShadowMap, ShadowSampler), vec3(ShadowCoords + Offset, ReceiverBiased));
    }

    return Shadow / float(SHADOW_SAMPLES);
}

float DistributionGGX(float cosTheta, float alpha)
{
    // Standard GGX/Trowbridge-Reitz distribution - optimized form
    float a = cosTheta * alpha;
    float k = alpha / (1.0 - cosTheta * cosTheta + a * a);
    return k * k * (1.0 / PI);
}

float GeometryGGX(float NdotL, float NdotV, float roughness)
{
    // Hammon's optimized approximation for GGX Smith geometry term
    // This version is an efficient approximation that:
    // 1. Avoids expensive square root calculations
    // 2. Combines both G1 terms into a single expression
    // 3. Provides very close results to the exact version at a much lower cost
    // SEE: https://www.gdcvault.com/play/1024478/PBR-Diffuse-Lighting-for-GGX
    return 0.5 / mix(2.0 * NdotL * NdotV, NdotL + NdotV, roughness);
}

float SchlickFresnel(float u)
{
    float m = 1.0 - u;
    float m2 = m * m;
    return m2 * m2 * m; // pow(m,5)
}

float Diffuse(float cLdotH, float cNdotV, float cNdotL, float roughness)
{
    float FD90_minus_1 = 2.0 * cLdotH * cLdotH * roughness - 0.5;
    float FdV = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotV);
    float FdL = 1.0 + FD90_minus_1 * SchlickFresnel(cNdotL);

    return (1.0 / PI) * (FdV * FdL * cNdotL); // Diffuse BRDF (Burley)
}

vec3 Specular(vec3 F0, float cLdotH, float cNdotH, float cNdotV, float cNdotL, float roughness)
{
    roughness = max(roughness, 1e-3);

    float alphaGGX = roughness * roughness;
    float D = DistributionGGX(cNdotH, alphaGGX);
    float G = GeometryGGX(cNdotL, cNdotV, alphaGGX);

    float cLdotH5 = SchlickFresnel(cLdotH);
    float F90 = clamp(50.0 * F0.g, 0.0, 1.0);
    vec3 F = F0 + (F90 - F0) * cLdotH5;

    return cNdotL * D * F * G; // Specular BRDF (Schlick GGX)
}

vec3 ComputeF0(float metallic, float specular, vec3 albedo)
{
    float dielectric = 0.16 * specular * specular;
    // use (albedo * metallic) as colored specular reflectance at 0 angle for metallic materials
    // SEE: https://google.github.io/filament/Filament.md.html
    return mix(vec3(dielectric), albedo, vec3(metallic));
}

const float ROUGHNESS = 0.3;
const float METALLIC = 0.8;
const vec3 AMBIENT = vec3(0.01);

void main()
{
    float CameraDistance = distance(CameraPosition, InPosition);

    vec4 Result = vec4(0.0, 0.0, 0.0, 1.0);

    vec3 BaseColor = (InNormalVS + 0.5) * 0.5;

    vec3 FragNormal = normalize(InNormal);
    vec3 FragViewDir = normalize(CameraPosition - InPosition);

    float NdotV = dot(FragNormal, FragViewDir);
    float cNdotV = max(NdotV, 1e-4);

    vec3 F0 = ComputeF0(METALLIC, 0.5, BaseColor);

    vec3 TotalDiffuse = vec3(0.0);
    vec3 TotalSpecular = vec3(0.0);

    for (uint Index = 0; Index < PointLights.length(); ++Index)
    {
        SGPUPointLight Light = PointLights[Index];

        vec3 LightToFrag = Light.Position.xyz - InPosition;
        vec3 FragToLight = normalize(LightToFrag);
        float LightToFragDistance = length(LightToFrag);
        if (LightToFragDistance > Light.Radius) continue;

        float Shadow = PointPCSS(PointShadowMaps[Index], Light, InPosition, FragNormal);
        float Attenuation = max(0.0, PointAttenuate(LightToFragDistance, Light));

        float NdotL = max(dot(FragNormal, FragToLight), 0.0);
        float cNdotL = min(NdotL, 1.0);
        vec3 H = normalize(FragViewDir + FragToLight);
        float LdotH = max(dot(FragToLight, H), 0.0);
        float cLdotH = min(dot(FragToLight, H), 1.0);
        float NdotH = max(dot(FragNormal, H), 0.0);
        float cNdotH = min(NdotH, 1.0);

        vec3 LightColorEnergy = Light.Color * Light.Energy;
        vec3 DiffLight = LightColorEnergy * Diffuse(cLdotH, cNdotV, cNdotL, ROUGHNESS);
        vec3 SpecLight = Specular(F0, cLdotH, cNdotH, cNdotV, cNdotL, ROUGHNESS);
        SpecLight *= LightColorEnergy * Light.Specular;

        TotalDiffuse += DiffLight * Shadow * Attenuation;
        TotalSpecular += SpecLight * Shadow * Attenuation;
    }

    for (uint Index = 0; Index < SpotLights.length(); ++Index)
    {
        SGPUSpotLight Light = SpotLights[Index];

        vec3 LightPosition = Light.Transform[3].xyz;
        vec3 FragToLight = normalize(LightPosition - InPosition);

        float Shadow = SpotPCSS(SpotShadowMaps[Index], Light, InPosition, FragNormal);
        float Attenuation = SpotAttenuate(FragToLight, Light);

        float NdotL = max(dot(FragNormal, FragToLight), 0.0);
        float cNdotL = min(NdotL, 1.0);
        vec3 H = normalize(FragViewDir + FragToLight);
        float LdotH = max(dot(FragToLight, H), 0.0);
        float cLdotH = min(dot(FragToLight, H), 1.0);
        float NdotH = max(dot(FragNormal, H), 0.0);
        float cNdotH = min(NdotH, 1.0);

        vec3 LightColorEnergy = Light.Color * Light.Energy;
        vec3 DiffLight = LightColorEnergy * Diffuse(cLdotH, cNdotV, cNdotL, ROUGHNESS);
        vec3 SpecLight = Specular(F0, cLdotH, cNdotH, cNdotV, cNdotL, ROUGHNESS);
        SpecLight *= LightColorEnergy * Light.Specular;

        TotalDiffuse += DiffLight * Shadow * Attenuation;
        TotalSpecular += SpecLight * Shadow * Attenuation;
    }

    TotalDiffuse = BaseColor * (TotalDiffuse + AMBIENT);

    OutColor = vec4(TotalDiffuse + TotalSpecular, 1.0);
    // OutColor.rgb = BaseColor;
}
