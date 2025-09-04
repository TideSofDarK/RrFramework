#version 450

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 View;
    mat4 Projection;
};

layout(set = 0, binding = 1) uniform samplerCube UniformCube;

void main()
{
    OutColor = vec4(0.0, 0.0, 0.0, 1.0);
    OutColor.r = texture(UniformCube, InPosition).r / 100.0;
}
