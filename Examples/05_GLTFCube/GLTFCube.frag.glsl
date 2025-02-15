#version 450

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 1) uniform sampler Sampler;
layout(set = 0, binding = 2) uniform texture2D ColorTexture;

void main()
{
    OutColor = vec4(texture(sampler2D(ColorTexture, Sampler), InUV).rgb, 1.0f);
}
