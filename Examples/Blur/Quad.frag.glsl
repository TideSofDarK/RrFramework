#version 460

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform sampler2D Image2D;

void main()
{
    OutColor = texture(Image2D, InUV);
}
