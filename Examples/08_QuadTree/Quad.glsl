struct SGPUDraw
{
    float X;
    float Y;
    float Width;
    float Height;
    int Type;
    uint Color;
    float Param1;
    float Param2;
};

layout(set = 0, binding = 0) uniform UniformGlobals
{
    mat4 ViewProjection;
    ivec2 ScreenSize;
};

layout(set = 0, binding = 1) readonly buffer StorageDraws
{
    SGPUDraw Draws[];
};
