#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InColor;

layout(location = 0) out vec3 OutColor;

void main()
{
    gl_Position = vec4(InPosition, 1.0);
    OutColor = InColor;
}
