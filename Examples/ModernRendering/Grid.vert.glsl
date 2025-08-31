#version 450

layout(location = 0) out vec3 OutNear;
layout(location = 1) out vec3 OutFar;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 View;
    mat4 Projection;
    float Near;
    float Far;
    float GridSmall;
    float GridBig;
};

const vec2 Positions[6] = vec2[6](
        vec2(1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, 1.0),
        vec2(1.0, -1.0));

vec3 UnprojectPoint(in vec3 Point)
{
    mat4 InvView = inverse(View);
    mat4 InvProj = inverse(Projection);
    vec4 UnprojectedPoint = InvView * InvProj * vec4(Point, 1.0);
    return UnprojectedPoint.xyz / UnprojectedPoint.w;
}

void main()
{
    vec2 Position = Positions[gl_VertexIndex];
    OutNear = UnprojectPoint(vec3(Position, 0.0));
    OutFar = UnprojectPoint(vec3(Position, 1.0));
    gl_Position = vec4(Position, 0.0, 1.0);
    gl_Position.y *= -1.0;
}
