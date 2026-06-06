#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;

layout(location = 0) out vec2 OutColor;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 ViewProjection;
    vec3 LightPosition;
    float FarPlane;
};

layout(set = 2, binding = 0) uniform SGPUMaterial
{
    uint AlphaMode;
    float AlphaCutoff;
    float Padding0;
    float Padding1;
} Material;
layout(set = 2, binding = 1) uniform sampler2D ColorTexture;
layout(set = 2, binding = 2) uniform sampler2D NormalTexture;
layout(set = 2, binding = 3) uniform sampler2D RoughnessMetallicTexture;

void main()
{
    vec4 Color = texture(ColorTexture, InUV);

    float Alpha = Color.a;
    if (Material.AlphaMode == 1 && Alpha < Material.AlphaCutoff)
    {
        discard;
    }

    float Distance = length(InPosition - LightPosition);
    OutColor.x = Distance;
    OutColor.y = Distance * Distance;
}
