#version 450

layout(location = 0) out vec2 OutTexCoord;

layout(set = 0, binding = 0) uniform UGlobals
{
    mat4 Projection;
    vec2 ImageSize;
};

void main()
{
    const vec2 POSITIONS[6] = vec2[](vec2(-1, 1), vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, -1), vec2(1, 1));
    vec2 Position = POSITIONS[gl_VertexIndex];
    gl_Position = Projection * vec4(Position * ImageSize * 0.5, 0.0, 1.0);
    OutTexCoord = (Position + 1) * 0.5;
}
