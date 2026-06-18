#version 460

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in flat uint InInstanceIndex;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform UGlobals
{
    mat4 View;
    mat4 Projection;
    float Time;
};

struct SGPUModel
{
    mat4 Model;
    vec4 Color;
};

layout(set = 1, binding = 0) readonly buffer UModels
{
    SGPUModel Models[];
};

layout(set = 3, binding = 0) uniform sampler2D ColorTexture;

void main()
{
    vec4 Color = texture(ColorTexture, InUV);
    if (Color.a < 0.5) discard;
    vec3 ViewNormal = inverse(View)[2].xyz;
    if (!gl_FrontFacing)
    {
        ViewNormal *= -1.0;
    }
    float Dot = clamp(dot(InNormal, ViewNormal), 0.5, 1.0);
    OutColor.rgb = Models[InInstanceIndex].Color.rgb * Color.rgb;
    OutColor.rgb = OutColor.rgb * Dot;
    OutColor.a = 1.0f;
}
