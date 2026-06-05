#version 460

layout(set = 0, binding = 0) uniform Globals
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
    float Near;
    float Far;
};

layout(location = 0) out vec4 OutColor;

void main()
{
    OutColor = vec4(1.0);
}
