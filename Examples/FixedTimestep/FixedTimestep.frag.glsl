#version 460

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 Projection;
    mat4 Model;
    float Time;
    float Aspect;
    vec2 Params;
};
layout(set = 0, binding = 1) uniform sampler2D Image;

void main()
{
    OutColor = texture(Image, InUV);
}
