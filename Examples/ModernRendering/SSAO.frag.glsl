#version 450

#define PI 3.1415926

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform sampler2D ColorImage;
layout(set = 0, binding = 1) uniform sampler2D DepthImage;

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

void main()
{
    OutColor = texture(ColorImage, InUV);

    const float DepthRange = (100.0 - 0.1);
    const vec2 DepthParams = vec2((0.1 - 100.0) / (0.1 * 100.0), 1.0 / 0.1);
    float Depth = texture(DepthImage, InUV).r;
    float LinearDepth = LinearizeDepth(DepthParams, Depth);
    // float DistanceDepth = LinearDepth / DepthRange;

    float RotationAngle = Hash12(gl_FragCoord.xy) * 2.0 * PI;
    float RCos = cos(RotationAngle);
    float RSin = sin(RotationAngle);

    float Shadow = 0.0;

    const int SAMPLES = 16;
    const float SCALE_STEP = 1.0 + 2.4 / float(SAMPLES);
    float Scale = 0.01;
    float Accessibility = 0;

    for (int Index = 0; Index < 16; ++Index)
    {
        vec2 RotatedOffset = Rotate2D(POISSON16[Index], RCos, RSin);
        vec2 SamplePos = InUV + RotatedOffset * Scale;
        Scale *= SCALE_STEP;

        float SampledDepth = texture(DepthImage, SamplePos.xy).r;
        SampledDepth = LinearizeDepth(DepthParams, SampledDepth);

        float Threshold = LinearDepth + (RotatedOffset.x * Scale * 2.0);

        float RangeIsInvalid = clamp(((LinearDepth -
                    SampledDepth) / SampledDepth), 0.0, 1.0);
        Accessibility += mix(float(SampledDepth > Threshold), 0.5,
                RangeIsInvalid);
    }

    Accessibility = Accessibility / float(SAMPLES);

    float SSAO = clamp(Accessibility, 0.0, 1.0);

    OutColor.rgb *= SSAO;
    // OutColor.rgb = vec3(SSAO);
}
