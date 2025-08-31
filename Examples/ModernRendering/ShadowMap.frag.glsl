#version 450

layout(location = 0) in vec3 InPosition;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 ViewProjection;
    vec3 LightPosition;
    float FarPlane;
};

void main()
{
    gl_FragDepth = length(InPosition) / FarPlane;
}
