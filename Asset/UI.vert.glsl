#version 460

layout(constant_id = 0) const uint CONVERT_TO_SRGB = 0;

layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec2 ScreenSize;
};

layout(set = 0, binding = 1) uniform sampler2D Atlas;

layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec4 OutColor;

float ToSRGBChannel(float Value)
{
    return Value <= 0.04045f ? Value / 12.92f
                             : pow((Value + 0.055f) / 1.055f, 2.4f);
}

void main()
{
    gl_Position = vec4((InPosition / ScreenSize) * 2.0 - 1.0, 0.0, 1.0);
    OutUV = InUV;
    if (CONVERT_TO_SRGB == 1)
    {
        OutColor = vec4(ToSRGBChannel(InColor.r), ToSRGBChannel(InColor.g), ToSRGBChannel(InColor.b), InColor.a);
    }
    else
    {
        OutColor = InColor;
    }
}
