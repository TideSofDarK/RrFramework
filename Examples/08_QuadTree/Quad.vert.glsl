#version 450
#extension GL_ARB_shading_language_include : require

#include "Quad.glsl"

layout(location = 0) out vec2 OutUV;
layout(location = 1) out uint OutIndex;

const vec2 Positions[4] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
const vec2 UVs[4] = vec2[](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
const uint Indices[6] = uint[](0, 1, 2, 2, 3, 0);

void main()
{
    SGPUDraw Draw = Draws[gl_InstanceIndex];
    vec2 VertexPosition = Positions[Indices[gl_VertexIndex]];
    vec2 Position = vec2(VertexPosition.x * Draw.Width, VertexPosition.y * Draw.Height);
    Position.x += Draw.X;
    Position.y += Draw.Y;
    gl_Position = ViewProjection * vec4(Position, 0.0, 1.0);
    OutUV = UVs[Indices[gl_VertexIndex]];
    OutIndex = gl_InstanceIndex;
}
