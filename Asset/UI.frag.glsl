#version 460

layout(constant_id = 0) const uint CONVERT_TO_SRGB = 0;

layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec2 ScreenSize;
};

layout(set = 0, binding = 1) uniform sampler2D Atlas;

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

float ToSRGBChannel(float Value)
{
    return Value <= 0.04045f ? Value / 12.92f : pow((Value + 0.055f) / 1.055f, 2.4f);
}

float ToLinearChannel(float Value)
{
    return Value <= 0.0031308f ? Value * 12.92f : pow(Value, 1.0f / 2.4f) * 1.055f - 0.055f;
}

void main() {
    OutColor = InColor;
    if (CONVERT_TO_SRGB == 1)
    {
        OutColor.r = ToSRGBChannel(OutColor.r);
        OutColor.g = ToSRGBChannel(OutColor.g);
        OutColor.b = ToSRGBChannel(OutColor.b);
        OutColor.a = ToSRGBChannel(OutColor.a);
    }
    if (CONVERT_TO_SRGB == 0)
    {
        OutColor.r = ToLinearChannel(OutColor.r);
        OutColor.g = ToLinearChannel(OutColor.g);
        OutColor.b = ToLinearChannel(OutColor.b);
        OutColor.a = ToLinearChannel(OutColor.a);
    }
    OutColor *= texture(Atlas, InUV);
}
