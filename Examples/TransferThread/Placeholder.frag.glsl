#version 450
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec2 InUV;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform SData {
    float Time;
    uint ImageCount;
};

void main()
{
    vec2 UV = InUV;
    UV /= 0.25;
    UV.x /= dFdx(InUV.x) / dFdy(InUV.y);
    float D = length(UV);
    float A = smoothstep(0.59, 0.61, D);
    float A2 = 1.0 - smoothstep(0.94, 0.95, D);
    float A4 = ((atan(UV.y, UV.x) / 3.14159) + 1.0) / 2.0;
    float A5 = fract(A4 + Time / 2.0);
    float A6 = (A5 * 2.0) - 1.0;
    float A7 = smoothstep(0.1, 1.0, abs(A6));
    float A10 = A2 * A * A7;
    OutColor = vec4(vec3(1.0), A10);
}
