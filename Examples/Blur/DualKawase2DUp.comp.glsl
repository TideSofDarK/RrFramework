#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 32;

layout(local_size_x_id = 0, local_size_y_id = 0, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D SrcImage;
layout(set = 0, binding = 1, rgba32f) writeonly uniform image2D DstImage;
layout(set = 0, binding = 2) uniform SGPUUniform {
    uvec2 SrcSize;
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
    uvec2 DstSize = SrcSize * 2;

    if (Coords.x < DstSize.x && Coords.y < DstSize.y)
    {
        vec2 Offset = TexelSizeUV * SamplePosMultiplier;
        vec2 HalfOffset = Offset * 0.5;

        vec2 CenterUV = vec2(Coords) * TexelSizeUV * 0.5;
        CenterUV += TexelSizeUV * 0.25;

        vec2 MaxUV = (vec2(SrcSize) - 0.5) * TexelSizeUV;

        vec3 Color = vec3(0.0);

        Color += Sample(MaxUV, CenterUV + vec2(0.0, -Offset.y)).rgb;
        Color += Sample(MaxUV, CenterUV + vec2(-Offset.x, 0.0)).rgb;
        Color += Sample(MaxUV, CenterUV + vec2(Offset.x, 0.0)).rgb;
        Color += Sample(MaxUV, CenterUV + vec2(0.0, Offset.y)).rgb;

        Color += Sample(MaxUV, CenterUV + vec2(-HalfOffset.x, -HalfOffset.y)).rgb * 2.0;
        Color += Sample(MaxUV, CenterUV + vec2(HalfOffset.x, -HalfOffset.y)).rgb * 2.0;
        Color += Sample(MaxUV, CenterUV + vec2(-HalfOffset.x, HalfOffset.y)).rgb * 2.0;
        Color += Sample(MaxUV, CenterUV + vec2(HalfOffset.x, HalfOffset.y)).rgb * 2.0;

        imageStore(DstImage, Coords, vec4(Color / 12.0, 1.0));
    }
}
