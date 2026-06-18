#version 460

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform UGlobals
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
    float Time;
};
layout(set = 0, binding = 1) uniform sampler Sampler;
layout(set = 0, binding = 2) uniform texture2D ColorTexture;

void main()
{
    vec2 A = InUV * 2.0 - 1.0;
    A /= pow(length(A), cos(Time));
    A = (A + 1.0) / 2.0;
    OutColor = vec4(texture(sampler2D(ColorTexture, Sampler), A).rgb, 1.0f);
}
