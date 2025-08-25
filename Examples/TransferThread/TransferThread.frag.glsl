#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SData {
    float Time;
    uint ImageCount;
};
layout(set = 0, binding = 1) uniform sampler2D Image;

void main()
{
    OutColor = texture(Image, InUV);
}
