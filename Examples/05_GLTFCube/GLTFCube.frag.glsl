#version 450

layout(location = 0) in vec3 InNormal;

layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = vec4(InNormal, 1.0f);
}
