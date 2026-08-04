#version 460

layout(constant_id = 0) const uint CONVERT_TO_SRGB = 0;

layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec2 CanvasExtent;
};

layout(set = 0, binding = 1) uniform sampler2D Atlas;

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

// float ToSRGBChannel(float Value)
// {
//     return Value <= 0.04045 ? Value / 12.92 : pow((Value + 0.055) / 1.055, 2.4);
// }

// vec3 ToSRGBColor(vec3 Color)
// {
//     Color.r = ToSRGBChannel(Color.r);
//     Color.g = ToSRGBChannel(Color.g);
//     Color.b = ToSRGBChannel(Color.b);

//     return Color;
// }
// vec3 LinearToSRGB(vec3 rgb)
// {
//     // See https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
//     return mix(1.055 * pow(rgb, vec3(1.0 / 2.4)) - 0.055,
//         rgb * 12.92,
//         lessThanEqual(rgb, vec3(0.0031308)));
// }

float sRGBToLinear(float rgb)
{
    // See https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
    return mix(pow((rgb + 0.055) * (1.0 / 1.055), 2.4),
        rgb * (1.0 / 12.92),
        rgb <= 0.04045);
}
vec3 sRGBToLinear(vec3 rgb)
{
    // See https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl
    return mix(pow((rgb + 0.055) * (1.0 / 1.055), vec3(2.4)),
        rgb * (1.0 / 12.92),
        lessThanEqual(rgb, vec3(0.04045)));
}

// vec3 rec709_eotf(vec3 v_) {
//     mat2x3 v = mat2x3((v_ / 4.5), pow((v_ + 0.0999) / 1.099, vec3(2.22222)));
//     return vec3(
//         v[v_[0] < 0.081 ? 0 : 1][0],
//         v[v_[1] < 0.081 ? 0 : 1][1],
//         v[v_[2] < 0.081 ? 0 : 1][2]);
// }
const float gamma = 2.2;

float toLinear(float v) {
    return pow(v, gamma);
}

vec2 toLinear(vec2 v) {
    return pow(v, vec2(gamma));
}

vec3 toLinear(vec3 v) {
    return pow(v, vec3(gamma));
}

vec4 toLinear(vec4 v) {
    return vec4(toLinear(v.rgb), v.a);
}
float toGamma(float v) {
    return pow(v, 1.0 / gamma);
}

vec2 toGamma(vec2 v) {
    return pow(v, vec2(1.0 / gamma));
}

vec3 toGamma(vec3 v) {
    return pow(v, vec3(1.0 / gamma));
}

vec4 toGamma(vec4 v) {
    return vec4(toGamma(v.rgb), v.a);
}

void main() {
    vec4 Color = InColor;
    vec4 Texture = texture(Atlas, InUV);
    if (CONVERT_TO_SRGB == 1)
    {
        Color.rgb = toLinear(Color.rgb);
        OutColor = Color * Texture;
        OutColor.rgb *= OutColor.a;
        OutColor.a = 1.0 - toLinear(1.0 - OutColor.a);
    }
    else
    {
        OutColor = Color * Texture;
    }
}
