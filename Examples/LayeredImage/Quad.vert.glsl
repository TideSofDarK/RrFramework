#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) out vec2 OutUV;

const vec2 Positions[6] = vec2[6](
        vec2(1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, 1.0),
        vec2(1.0, -1.0));

void main()
{
    vec2 Position = Positions[gl_VertexIndex];
    gl_Position = vec4(Position, 0.0, 1.0);
    OutUV = (Position + 1.0) / 2.0;
}
