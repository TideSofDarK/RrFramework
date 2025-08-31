#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec3 InNormal;

layout(location = 0) out vec3 OutPosition;

layout(set = 0, binding = 1) readonly buffer SGPUStorage
{
    mat4 ViewProjection;
};

void main()
{
    gl_Position = ViewProjection * vec4(InPosition, 1.0);
    OutPosition = gl_Position.xyz;
}
