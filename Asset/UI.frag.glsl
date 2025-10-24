#version 460

layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec2 ScreenSize;
};

layout(set = 0, binding = 1) uniform sampler2D Atlas;

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

void main() {
    OutColor = InColor * texture(Atlas, InUV);
}
