#version 450
#extension GL_ARB_shading_language_include : require

#include "Skybox.glsl"

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec3 OutPosition;

void main()
{
    OutPosition = InPosition;
    mat4 Temp = View;
    Temp[3] = vec4(0.0, 0.0, 0.0, 1.0);
    gl_Position = Projection * Temp * vec4(InPosition, 1.0);
}
