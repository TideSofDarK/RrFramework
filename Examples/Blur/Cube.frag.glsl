#version 460

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
    OutColor = texture(UniformCube, InPosition);
}
