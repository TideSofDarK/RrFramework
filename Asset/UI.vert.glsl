#version 460

layout(constant_id = 0) const uint CONVERT_TO_SRGB = 0;

layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec2 CanvasExtent;
};

layout(set = 0, binding = 1) uniform sampler2D Atlas;

layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec4 OutColor;

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

void main()
{
    gl_Position = vec4((InPosition / CanvasExtent) * 2.0 - 1.0, 0.0, 1.0);
    OutUV = InUV;
    OutColor = InColor;
    if (CONVERT_TO_SRGB == 1)
    {
        // OutColor.rgb *= OutColor.a;
        // OutColor.rgb = sRGBToLinear(OutColor.rgb);
    }
}
