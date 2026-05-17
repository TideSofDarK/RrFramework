#version 460

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
    float Time;
};
layout(set = 0, binding = 1) uniform sampler Sampler;
layout(set = 0, binding = 2) uniform texture2D ColorTexture;

// layout(set = 0, binding = 3) uniform sampler2D bb03;
// layout(set = 0, binding = 4) readonly buffer Lbb04
// {
//     mat4 Model;
// } bb04;

// layout(set = 0, binding = 5) buffer Lbb05
// {
//     mat4 Elements[];
// } bb05;

// layout(rgba8, set = 0, binding = 6) uniform image2D bb06;

// layout(rgba8, set = 0, binding = 7) uniform image2DArray bb07[5];

void main()
{
    vec2 A = InUV * 2.0 - 1.0;
    A /= pow(length(A), cos(Time));
    A = (A + 1.0) / 2.0;
    OutColor = vec4(texture(sampler2D(ColorTexture, Sampler), A).rgb, 1.0f);
}
