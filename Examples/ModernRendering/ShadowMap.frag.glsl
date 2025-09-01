#version 450

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec2 OutMoments;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 ViewProjection;
    vec3 LightPosition;
    float FarPlane;
};

vec2 ComputeMoments(in float Depth)
{
    vec2 Moments;
    Moments.x = Depth;

    #if 0

    Moments.y = Depth * Depth +
            0.25 * (dFdx(Depth) * dFdx(Depth) + dFdy(Depth) * dFdy(Depth));

    #else

    Moments.y = Depth * Depth;

    #endif

    return Moments;
}

void main()
{
    float Depth = length(InPosition - LightPosition);
    OutMoments = ComputeMoments(Depth);
}
