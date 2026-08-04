#version 460

layout(set = 0, binding = 0) uniform UniformGlobals
{
    vec4 CanvasExtent;
    uint IndexCount;
    uint Reserved1;
    uint Reserved2;
    uint Reserved3;
};

struct Rr_UI2Vertex
{
    vec2 Offset;
    vec2 UV;
    uint Color;
    uint ClipIndex;
    uint NoFloor;
    uint Reserved0;
};

layout(set = 0, binding = 2) readonly buffer BVertices
{
    Rr_UI2Vertex Vertices[];
};

layout(set = 0, binding = 3) readonly buffer BIndices
{
    uint Indices[];
};

layout(location = 0) out vec2 OutPosition;
layout(location = 1) out vec2 OutUV;
layout(location = 2) out vec4 OutColor;
layout(location = 3) out flat uint OutClipIndex;

void main()
{
    uint Index = Indices[gl_VertexIndex];
    Rr_UI2Vertex Vertex = Vertices[Index];
    if (Vertex.NoFloor == 0)
    {
        Vertex.Offset = floor(Vertex.Offset);
    }

    gl_Position = vec4((Vertex.Offset / CanvasExtent.xy) * 2.0 - 1.0, 0.0, 1.0);

    OutPosition = Vertex.Offset;
    OutUV = Vertex.UV;
    OutColor.r = float((0xFF000000 & Vertex.Color) >> 24) / 255.0;
    OutColor.g = float((0x00FF0000 & Vertex.Color) >> 16) / 255.0;
    OutColor.b = float((0x0000FF00 & Vertex.Color) >> 8) / 255.0;
    OutColor.a = float((0x000000FF & Vertex.Color) >> 0) / 255.0;
    OutClipIndex = Vertex.ClipIndex;
}
