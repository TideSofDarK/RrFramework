#version 450

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec2 OutColor;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 ViewProjection;
    vec3 LightPosition;
    float FarPlane;
};

void main()
{
    float Distance = length(InPosition - LightPosition);
    OutColor.x = Distance;
    OutColor.y = Distance * Distance;
}
