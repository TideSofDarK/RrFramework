#version 450

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec3 OutPosition;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 View;
    mat4 Projection;
    float Time;
};

layout(set = 0, binding = 1) uniform samplerCube UniformCube;

void main()
{
    OutPosition = InPosition;
    mat4 Temp = View;
    Temp[3] = vec4(0.0, 0.0, 0.0, 1.0);
    gl_Position = Projection * Temp * vec4(InPosition, 1.0);
}
