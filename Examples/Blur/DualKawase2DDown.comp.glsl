#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 32;

layout(local_size_x_id = 0, local_size_y_id = 0, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D SrcImage;
layout(set = 0, binding = 1, rgba8) writeonly uniform image2D DstImage;
layout(set = 0, binding = 2) uniform SGPUUniform {
    ivec2 SrcSize;
    vec2 TexelSizeUV;
    float SamplePosMultiplier;
};

vec3 Sample(vec2 MaxUV, vec2 UV)
{
    return texture(SrcImage, clamp(UV, vec2(0.0), MaxUV)).rgb;
}

void main()
{
    ivec2 Coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 DstSize = SrcSize / 2;

    if (Coords.x < DstSize.x && Coords.y < DstSize.y)
    {
        vec2 Offset = TexelSizeUV * SamplePosMultiplier;

        vec2 CenterUV = vec2(Coords) * TexelSizeUV * 2.0;
        CenterUV += TexelSizeUV;

        vec2 MaxUV = (vec2(SrcSize) - 0.5) * TexelSizeUV;

        vec3 Color = Sample(MaxUV, CenterUV).rgb * 4.0;

        Color += Sample(MaxUV, CenterUV + vec2(-Offset.x, -Offset.y)).rgb;
        Color += Sample(MaxUV, CenterUV + vec2(Offset.x, -Offset.y)).rgb;
        Color += Sample(MaxUV, CenterUV + vec2(-Offset.x, Offset.y)).rgb;
        Color += Sample(MaxUV, CenterUV + vec2(Offset.x, Offset.y)).rgb;

        imageStore(DstImage, Coords, vec4(Color / 8.0, 1.0));
    }
}
