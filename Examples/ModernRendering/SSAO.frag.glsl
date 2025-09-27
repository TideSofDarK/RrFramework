#version 450

/* Scalable Ambient Obscurance https://research.nvidia.com/sites/default/files/pubs/2012-06_Scalable-Ambient-Obscurance/McGuire12SAO.pdf */
/* Implementation adapted from https://github.com/mrdoob/three.js/pull/11458/files */

#define PI 3.1415926
#define PI_2 (PI * 2.0)
#define NUM_SAMPLES 7
#define INV_NUM_SAMPLES (1.0 / float(NUM_SAMPLES))
#define NUM_RINGS 4
#define ANGLE_STEP (PI_2 * float(NUM_RINGS) / float(NUM_SAMPLES))

layout(location = 0) in vec2 InUV;

layout(location = 0) out float OutAO;

layout(set = 0, binding = 0) uniform sampler2D NormalDepthImage;
layout(set = 0, binding = 1) uniform SGPUUniform {
    mat4 Projection;
    mat4 InvProjection;
    float Bias;
    float Intensity;
    float Scale;
    float KernelRadius;
    float MinRes;
    float CameraNear;
    float CameraFar;
    float DepthRange;
    vec2 DepthParams;
    vec2 ScreenRes;
    float BlurSharpness;
};

float Hash12(vec2 Seed)
{
    vec3 Seed3 = fract(vec3(Seed.xyx) * .1031);
    Seed3 += dot(Seed3, Seed3.yzx + 33.33);
    return fract((Seed3.x + Seed3.y) * Seed3.z);
}

vec2 Rotate2D(vec2 Vec, float Cos, float Sin)
{
    return vec2(Vec.x * Cos - Vec.y * Sin, Vec.x * Sin + Vec.y * Cos);
}

float LinearizeDepth(in vec2 DepthParams, float Depth)
{
    return 1.0 / (Depth * DepthParams.x + DepthParams.y);
}

vec3 GetViewNormal(in vec3 ViewPosition)
{
    return normalize(cross(dFdx(ViewPosition), dFdy(ViewPosition)));
}

vec3 GetViewPosition(in vec2 ScreenPosition, in float Depth, in float ViewZ)
{
    float ClipW = Projection[2][3] * ViewZ + Projection[3][3];
    // vec4 ClipPosition = vec4((vec3(ScreenPosition, Depth) - 0.5) * 2.0, 1.0);
    vec4 ClipPosition = vec4((ScreenPosition - 0.5) * 2.0, Depth, 1.0);
    ClipPosition *= ClipW;
    return (InvProjection * ClipPosition).xyz;
}

float GetOcclusion(
    in vec3 CenterViewPosition,
    in vec3 CenterViewNormal,
    in vec3 SampleViewPosition,
    in float ScaleDividedByCameraFar,
    in float MinResTimesCameraFar)
{
    vec3 ViewDelta = SampleViewPosition - CenterViewPosition;
    float ViewDistance = length(ViewDelta);
    float ScaledScreenDistance = ScaleDividedByCameraFar * ViewDistance;

    return max(0.0, (dot(CenterViewNormal, ViewDelta) - MinResTimesCameraFar) / ScaledScreenDistance - Bias) / (1.0 + pow(ScaledScreenDistance, 2.0));
}

float GetAmbientOcclusion(in vec3 CenterViewPosition)
{
    float ScaleDividedByCameraFar = Scale / 100.0;
    float MinResTimesCameraFar = MinRes * 100.0;
    // vec3 CenterViewNormal = GetViewNormal(CenterViewPosition);
    vec3 CenterViewNormal = -texture(NormalDepthImage, InUV).rgb;

    float Angle = Hash12(InUV + gl_FragCoord.xy) * PI_2;
    vec2 Radius = vec2(KernelRadius * INV_NUM_SAMPLES) / ScreenRes;
    vec2 RadiusStep = Radius;

    float OcclusionSum = 0.0;
    float WeightSum = 0.0;

    for (int Index = 0; Index < NUM_SAMPLES; ++Index)
    {
        vec2 SampleUV = InUV + vec2(cos(Angle), sin(Angle)) * Radius;
        Radius += RadiusStep;
        Angle += ANGLE_STEP;

        float SampleDepth = texture(NormalDepthImage, SampleUV).a;
        if (SampleDepth >= (1.0 - 0.0001))
        {
            continue;
        }

        float SampleViewZ = LinearizeDepth(DepthParams, SampleDepth) * DepthRange;
        vec3 SampleViewPosition = GetViewPosition(SampleUV, SampleDepth, SampleViewZ);
        OcclusionSum += GetOcclusion(CenterViewPosition, CenterViewNormal, SampleViewPosition, ScaleDividedByCameraFar, MinResTimesCameraFar);

        WeightSum += 1.0;
    }

    if (WeightSum == 0.0) discard;

    return OcclusionSum * (Intensity / WeightSum);
}

void main()
{
    float CenterDepth = texture(NormalDepthImage, InUV).a;
    if (CenterDepth >= (1.0 - 0.0001))
    {
        discard;
    }

    float CenterLinearDepth = LinearizeDepth(DepthParams, CenterDepth);
    float CenterViewZ = CenterLinearDepth * DepthRange;
    vec3 ViewPosition = GetViewPosition(InUV, CenterDepth, CenterViewZ);

    OutAO = uintBitsToFloat(packHalf2x16(vec2(1.0 - GetAmbientOcclusion(ViewPosition), CenterLinearDepth)));
}
