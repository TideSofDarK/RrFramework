#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SData {
    float Time;
    uint ImageCount;
};
layout(set = 0, binding = 1) uniform sampler2DArray Images;

void main()
{
    float ImageCountF = float(ImageCount);
    float C = (InUV.x * 2.0) - 1.0;
    float Sign = sign(C);
    C = abs(C);
    C = clamp(C - 1.0 + abs(cos(Time)), 0.0, 1.0);
    C = abs(C) * Sign;
    float X = (C + 1.0) / 2.0;
    float Alpha = mod(X, 1.0 / ImageCountF) * ImageCountF;
    Alpha = smoothstep(0.9, 1.0, Alpha);
    float CurrentIndex = floor(X * ImageCountF);
    float NextIndex = min(CurrentIndex + 1.0, ImageCountF - 1.0);
    OutColor = mix(texture(Images, vec3(InUV, CurrentIndex)),
            texture(Images, vec3(InUV, NextIndex)),
            Alpha);
}
