#version 460

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform sampler2D Image;

vec3 Neutral(vec3 Color) {
    const float StartCompression = 0.8 - 0.04;
    const float Desaturation = 0.15;

    float X = min(Color.r, min(Color.g, Color.b));
    float Offset = X < 0.08 ? X - 6.25 * X * X : 0.04;
    Color -= Offset;

    float Peak = max(Color.r, max(Color.g, Color.b));
    if (Peak < StartCompression) return Color;

    const float D = 1.0 - StartCompression;
    float NewPeak = 1.0 - D * D / (Peak + D - StartCompression);
    Color *= NewPeak / Peak;

    float G = 1.0 - 1.0 / (Desaturation * (Peak - NewPeak) + 1.0);
    return mix(Color, vec3(NewPeak), G);
}

void main()
{
    // OutColor = texture(Image, InUV);
    OutColor.rgb = Neutral(texture(Image, InUV).rgb);
}
