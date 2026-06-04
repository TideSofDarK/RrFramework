#version 460

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform sampler2D Image;

void main()
{
    OutColor = texture(Image, InUV);
}
