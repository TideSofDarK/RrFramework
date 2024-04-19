#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_normal;

#include "Uber3D.glsl"

void main()
{
    Vertex v = u_draw.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = u_globals.proj * u_globals.intermediate * u_globals.view * u_draw.model * vec4(v.position, 1.0f);

    out_color = v.color;
    out_uv.x = v.uvX;
    out_uv.y = v.uvY;
    out_normal = (u_globals.view * u_draw.model * vec4(v.normal, 0.0f)).rgb;
}
