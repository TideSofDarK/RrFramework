#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SData {
    float Time;
    uint ImageCount;
};
layout(set = 0, binding = 1) uniform sampler3D Image;

void main()
{
    float ImageCountF = float(ImageCount);
    vec2 S = InUV * 2.0 - 1.0;
    float D = length(S) / sqrt(2.0);
    float B = floor(mod(Time, ImageCountF)) / (ImageCountF - 1.0);
    float B2 = floor(mod(Time - 1.0, ImageCountF)) / (ImageCountF - 1.0);
    float M = 1.0 - smoothstep(0.0, 0.1, D - fract(Time));
    OutColor = texture(Image, vec3(InUV, (M * B) + (1.0 - M) * B2));
}
