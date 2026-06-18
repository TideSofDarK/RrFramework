#version 460

layout(location = 0) out vec3 OutTexCoord;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 View;
    mat4 Projection;
};

layout(set = 0, binding = 1) uniform samplerCube UniformCube;

void main()
{
    vec2 POSITIONS[3] = vec2[](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));

    vec4 PositionCS = vec4(POSITIONS[gl_VertexIndex], 1, 1);
    vec4 PositionVS = inverse(Projection) * PositionCS;
    vec4 ViewDir = inverse(View) * vec4(PositionVS.xyz, 0);

    gl_Position = PositionCS;
    OutTexCoord = ViewDir.xyz;
}
