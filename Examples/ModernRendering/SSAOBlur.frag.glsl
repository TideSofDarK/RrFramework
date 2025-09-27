#version 460

const float KERNEL_RADIUS = 3;

layout(location = 0) in vec2 InUV;

layout(location = 0) out float OutAO;

layout(set = 0, binding = 0) uniform sampler2D AODepthImage;
layout(set = 0, binding = 1) uniform SGPUUniformBlur {
    vec2 InvResDir;
    float BlurSharpness;
};

vec2 Sample(vec2 UV)
{
    return unpackHalf2x16(floatBitsToUint(texture(AODepthImage, UV).r));
}

float BlurFunction(vec2 UV, float r, float CenterAO, float CenterDepth, inout float w_total)
{
    vec2 Packed = Sample(UV);

    const float BlurSigma = float(KERNEL_RADIUS) * 0.5;
    const float BlurFalloff = 1.0 / (2.0 * BlurSigma * BlurSigma);

    float ddiff = (Packed.g - CenterDepth) * BlurSharpness;
    float w = exp2(-r * r * BlurFalloff - ddiff * ddiff);
    w_total += w;

    return Packed.r * w;
}

void main()
{
    vec2 CenterPacked = Sample(InUV);

    float CenterAO = CenterPacked.r;
    float CenterDepth = CenterPacked.g;

    float TotalAO = CenterAO;
    float WTotal = 1.0;

    for (float r = 1; r <= KERNEL_RADIUS; ++r)
    {
        vec2 uv = InUV + InvResDir * r;
        TotalAO += BlurFunction(uv, r, CenterAO, CenterDepth, WTotal);
    }

    for (float r = 1; r <= KERNEL_RADIUS; ++r)
    {
        vec2 uv = InUV - InvResDir * r;
        TotalAO += BlurFunction(uv, r, CenterAO, CenterDepth, WTotal);
    }

    OutAO = uintBitsToFloat(packHalf2x16(vec2(TotalAO / WTotal, CenterDepth)));
}
