#version 460

layout(constant_id = 0) const uint LOCAL_SIZE = 32;

layout(local_size_x_id = 0, local_size_y_id = 0, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D SrcImage;
layout(set = 0, binding = 1, rgba8) restrict writeonly uniform image2D DstImage;

const float PI = 3.1415926;
const float LANCZOS_A = 3.0;

vec3 LinearToSRGB(vec3 RGB)
{
    return mix(1.055 * pow(RGB, vec3(1.0 / 2.4)) - 0.055,
        RGB * 12.92,
        lessThanEqual(RGB, vec3(0.0031308)));
}

float Lanczos(float X)
{
    if (X == 0.0)
    {
        return 1.0;
    }

    if (abs(X) >= LANCZOS_A)
    {
        return 0.0;
    }

    float B = PI * X;

    return (LANCZOS_A * sin(B) * sin(B / LANCZOS_A)) / (B * B);
}

void main()
{
    ivec2 Coords = ivec2(gl_GlobalInvocationID.xy);
    ivec2 DstSize = imageSize(DstImage);
    ivec2 SrcSize = DstSize * 2;
    if (Coords.x < DstSize.x && Coords.y < DstSize.y)
    {
        float FinalWeight = 0.0;
        vec4 FinalColor = vec4(0.0);
        vec2 SrcPixel = vec2(Coords * 2) + 1.0;
        vec2 SrcPixelCenter = floor(SrcPixel - 0.5) + 0.5;
        for (float Y = -LANCZOS_A + 1.0; Y <= LANCZOS_A; ++Y)
        {
            for (float X = -LANCZOS_A + 1.0; X <= LANCZOS_A; ++X)
            {
                vec2 SamplePos = SrcPixelCenter + vec2(Y, X);
                vec2 SampleCoord = SamplePos / vec2(SrcSize);
                vec2 Delta = SrcPixel - SamplePos;
                float Weight = Lanczos(Delta.x) * Lanczos(Delta.y);
                FinalWeight += Weight;
                FinalColor += texture(SrcImage, SampleCoord) * Weight;
            }
        }
        FinalColor /= FinalWeight;
        FinalColor.rgb = LinearToSRGB(FinalColor.rgb);
        FinalColor.a = 1.0;
        imageStore(DstImage, Coords, FinalColor);
    }
}
