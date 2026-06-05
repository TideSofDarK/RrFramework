#version 460

layout(set = 0, binding = 0) uniform Globals
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
    float Near;
    float Far;
};

layout(location = 0) in vec3 InPosition;

void main()
{
    gl_Position = Projection * View * Model * vec4(InPosition, 1.0);
}
