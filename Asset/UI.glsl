layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec2 ScreenSize;
    float DistanceRange;
    float Time;
};

layout(set = 0, binding = 1) uniform sampler2D Atlas;
