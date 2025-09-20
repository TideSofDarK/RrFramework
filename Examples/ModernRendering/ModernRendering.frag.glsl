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
layout(location = 1) out vec4 OutNormalDepth;

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
    float NearPlane;
    float FarPlane;
    vec2 Padding;
};

struct SGPUSpotLight
{
    mat4 Transform;
    mat4 ViewProjection;
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

float ZToDepthValue(in float Z, in float NearPlane, in float FarPlane)
{
    float FarPlusNear = FarPlane + NearPlane;
    float FarMinusNear = FarPlane - NearPlane;
    float FTimesNear = FarPlane * NearPlane;
    float ZRec = 1.0 / Z;
    return FarPlusNear / FarMinusNear + ZRec * ((-2.0f * FTimesNear) / FarMinusNear);
}

float VectorToDepthValue(in vec3 Vec, in float NearPlane, in float FarPlane)
{
    vec3 AbsVec = abs(Vec);
    float LocalZComp = max(AbsVec.x, max(AbsVec.y, AbsVec.z));
    float NormZComp = ZToDepthValue(LocalZComp, NearPlane, FarPlane);
    return (NormZComp + 1.0) * 0.5;
}

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
    float Receiver = VectorToDepthValue(FragToLight, Light.NearPlane, Light.FarPlane);
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

    vec4 FragLS = Light.ViewProjection * vec4(FragPosition + NormalOffsetBias, 1.0);
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

float DistributionGGX(float CosTheta, float Alpha)
{
    // Standard GGX/Trowbridge-Reitz distribution - optimized form
    float A = CosTheta * Alpha;
    float K = Alpha / (1.0 - CosTheta * CosTheta + A * A);
    return K * K * (1.0 / PI);
}

float GeometryGGX(float NDotL, float NDotV, float Roughness)
{
    // Hammon's optimized approximation for GGX Smith geometry term
    // This version is an efficient approximation that:
    // 1. Avoids expensive square root calculations
    // 2. Combines both G1 terms into a single expression
    // 3. Provides very close results to the exact version at a much lower cost
    // SEE: https://www.gdcvault.com/play/1024478/PBR-Diffuse-Lighting-for-GGX
    return 0.5 / mix(2.0 * NDotL * NDotV, NDotL + NDotV, Roughness);
}

float SchlickFresnel(float CosTheta)
{
    return pow(1.0 - CosTheta, 5.0);
}

float ComputeDiffuse(float LDotH, float NDotV, float NDotL, float Roughness)
{
    float FD90MinusOne = 2.0 * LDotH * LDotH * Roughness - 0.5;
    float FdV = 1.0 + FD90MinusOne * SchlickFresnel(NDotV);
    float FdL = 1.0 + FD90MinusOne * SchlickFresnel(NDotL);

    return (1.0 / PI) * (FdV * FdL * NDotL); // Diffuse BRDF (Burley)
}

vec3 ComputeSpecular(vec3 F0, float LDotH, float NDotH, float NDotV, float NDotL, float Roughness)
{
    Roughness = max(Roughness, 1e-3);

    float AlphaGGX = Roughness * Roughness;
    float D = DistributionGGX(NDotH, AlphaGGX);
    float G = GeometryGGX(NDotL, NDotV, AlphaGGX);

    float LDotH5 = SchlickFresnel(LDotH);
    float F90 = clamp(50.0 * F0.g, 0.0, 1.0);
    vec3 F = F0 + (F90 - F0) * LDotH5;

    return NDotL * D * F * G; // Specular BRDF (Schlick GGX)
}

vec3 ComputeF0(float Metallic, float Specular, vec3 Albedo)
{
    float Dielectric = 0.16 * Specular * Specular;
    // use (albedo * metallic) as colored specular reflectance at 0 angle for metallic materials
    // SEE: https://google.github.io/filament/Filament.md.html
    return mix(vec3(Dielectric), Albedo, vec3(Metallic));
}

struct SLightDots
{
    float NDotL;
    float LDotH;
    float NDotH;
};

SLightDots GetLightDots(in vec3 FragNormal, in vec3 FragViewDir, in vec3 FragToLight)
{
    SLightDots LightDots;

    float NDotL = max(dot(FragNormal, FragToLight), 0.0);
    LightDots.NDotL = min(NDotL, 1.0);

    vec3 H = normalize(FragViewDir + FragToLight);

    float LDotH = max(dot(FragToLight, H), 0.0);
    LightDots.LDotH = min(dot(FragToLight, H), 1.0);

    float NDotH = max(dot(FragNormal, H), 0.0);
    LightDots.NDotH = min(NDotH, 1.0);

    return LightDots;
}

const float ROUGHNESS = 0.2;
const float METALLIC = 0.8;
const vec3 AMBIENT = vec3(0.008);

void main()
{
    float CameraDistance = distance(CameraPosition, InPosition);

    vec4 Result = vec4(0.0, 0.0, 0.0, 1.0);

    vec3 BaseColor = (InNormalVS + 0.5) * 0.5;

    vec3 FragNormal = normalize(InNormal);
    vec3 FragViewDir = normalize(CameraPosition - InPosition);

    float NDotV = max(dot(FragNormal, FragViewDir), 1e-4);

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

        SLightDots Dots = GetLightDots(FragNormal, FragViewDir, FragToLight);

        vec3 LightColorEnergy = Light.Color * Light.Energy;
        vec3 DiffLight = LightColorEnergy * ComputeDiffuse(Dots.LDotH, NDotV, Dots.NDotL, ROUGHNESS);
        vec3 SpecLight = ComputeSpecular(F0, Dots.LDotH, Dots.NDotH, NDotV, Dots.NDotL, ROUGHNESS);
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

        SLightDots Dots = GetLightDots(FragNormal, FragViewDir, FragToLight);

        vec3 LightColorEnergy = Light.Color * Light.Energy;
        vec3 DiffLight = LightColorEnergy * ComputeDiffuse(Dots.LDotH, NDotV, Dots.NDotL, ROUGHNESS);
        vec3 SpecLight = ComputeSpecular(F0, Dots.LDotH, Dots.NDotH, NDotV, Dots.NDotL, ROUGHNESS);
        SpecLight *= LightColorEnergy * Light.Specular;

        TotalDiffuse += DiffLight * Shadow * Attenuation;
        TotalSpecular += SpecLight * Shadow * Attenuation;
    }

    TotalDiffuse = BaseColor * (TotalDiffuse + AMBIENT);

    OutColor = vec4(TotalDiffuse + TotalSpecular, 1.0);
    // OutColor.rgb = BaseColor;

    OutNormalDepth = vec4(normalize(InNormalVS), gl_FragCoord.z);
}
