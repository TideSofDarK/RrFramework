#version 450

layout(location = 0) in vec3 InNear;
layout(location = 1) in vec3 InFar;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 View;
    mat4 Projection;
    float Near;
    float Far;
    float GridSmall;
    float GridBig;
};

vec4 AddGrid(in vec3 FragPos, in float Size, in bool HighlightAxis)
{
    vec2 Coord = FragPos.xz * Size;
    vec2 Derivative = fwidth(Coord);
    vec2 Grid = abs(fract(Coord - 0.5) - 0.5) / Derivative;
    float Line = min(Grid.x, Grid.y);

    vec4 Color = vec4(0.2, 0.2, 0.2, 0.0);
    Color.a = 1.0 - min(Line, 1.0);

    if (HighlightAxis)
    {
        float MinZ = min(Derivative.y, 1.0);
        float MinX = min(Derivative.x, 1.0);
        if (FragPos.x > -MinX && FragPos.x < MinX)
        {
            Color.g = 1.0;
            Color.r = Color.b = 0;
            Color.a = 1.0 - min(Grid.x, 1.0);
        }
        if (FragPos.z > -MinZ && FragPos.z < MinZ)
        {
            Color.g = Color.b = 0;
            Color.r = 1.0;
            Color.a = 1.0 - min(Grid.y, 1.0);
        }
    }

    return Color;
}

void main()
{
    float Alpha = -InNear.y / (InFar.y - InNear.y);
    vec3 FragPos = InNear + (InFar - InNear) * Alpha;

    vec4 ClipSpacePos = Projection * View * vec4(FragPos.xyz, 1.0);
    gl_FragDepth = ClipSpacePos.z / ClipSpacePos.w;

    float ClipSpaceDepth = gl_FragDepth;
    float LinearDepth = (2.0 * Near * Far) / (Far + Near - ClipSpaceDepth * (Far - Near));
    LinearDepth /= Far;
    float Fading = max(0, (0.5 - LinearDepth));

    vec4 Color = AddGrid(FragPos, 1.0 / GridSmall, true) + AddGrid(FragPos, 1.0 / GridBig, false);
    Color.a *= float(Alpha > 0.0);
    Color.a *= Fading;
    OutColor = Color;
}
