#version 460

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec3 InPosition;
layout(location = 3) in vec3 InNormalVS;

layout(location = 0) out vec4 OutNormalDepth;

layout(set = 3, binding = 0) uniform SGPUMaterial
{
    uint AlphaMode;
    float AlphaCutoff;
    float Padding0;
    float Padding1;
} Material;
layout(set = 3, binding = 1) uniform sampler2D ColorTexture;
layout(set = 3, binding = 2) uniform sampler2D NormalTexture;
layout(set = 3, binding = 3) uniform sampler2D RoughnessMetallicTexture;

void main()
{
    vec4 Color = texture(ColorTexture, InUV);
    float Alpha = Color.a;
    if (Material.AlphaMode == 1 && Alpha < Material.AlphaCutoff)
    {
        discard;
    }

    OutNormalDepth = vec4(normalize(InNormalVS), gl_FragCoord.z);
}
