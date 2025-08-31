#version 450

layout(location = 0) in vec3 InPosition;

layout(set = 0, binding = 0) readonly buffer SGPUUniform
{
    vec4 LightPosition;
    float FarPlane;
};

void main()
{
    float Distance = distance(InPosition, LightPosition.xyz);
    Distance /= FarPlane;
    gl_FragDepth = Distance;
}
