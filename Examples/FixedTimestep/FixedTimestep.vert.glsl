#version 460

layout(location = 0) out vec2 OutUV;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 Projection;
    mat4 Model;
    float Time;
    float Aspect;
    vec2 Params;
};
layout(set = 0, binding = 1) uniform sampler2D Image;

const vec2 Positions[6] = vec2[6](
        vec2(0.5, 0.5),
        vec2(-0.5, -0.5),
        vec2(-0.5, 0.5),
        vec2(-0.5, -0.5),
        vec2(0.5, 0.5),
        vec2(0.5, -0.5));

void main()
{
    vec2 Position = Positions[gl_VertexIndex];
    gl_Position = Projection * Model * vec4(Position, 0.0f, 1.0f);
    OutUV = Position + 0.5;
}
