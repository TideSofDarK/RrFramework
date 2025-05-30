#version 450
#extension GL_ARB_shading_language_include : require

#include "Skybox.glsl"

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = texture(UniformCube, InPosition);
}
