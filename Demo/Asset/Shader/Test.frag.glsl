#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec3 InColor;
layout(location = 1) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform UTest {
    vec3 color;
} UniformTest;

layout(set = 0, binding = 1) uniform sampler2D UniformTexture;
void main()
{
    OutColor = vec4(texture(UniformTexture, InUV).rgb, 1.0f);
    // out_color = vec4(u_test.color, 1.0f);
}
