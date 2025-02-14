#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;

layout(location = 0) out vec3 OutNormal;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
};

void main()
{
    gl_Position = Projection * View * Model * vec4(InPosition, 1.0f);
    OutNormal = InNormal;
}
