#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in uint InSurfaceIndex;
layout(location = 2) in vec3 InNormal;
layout(location = 3) in uint InTextureIndex;

layout(location = 0) out vec3 OutNormal;
layout(location = 1) out vec2 OutTexCoord;
layout(location = 2) out flat uint OutTexIndex;

struct SGPUSurface
{
    vec3 VectorX;
    float DistanceX;
    vec3 VectorY;
    float DistanceY;
};

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
};
layout(set = 0, binding = 1) uniform sampler2D AtlasImage;
layout(set = 0, binding = 2) readonly buffer USurfaces
{
    SGPUSurface Surfaces[];
};

void main()
{
    gl_Position = Projection * View * Model * vec4(InPosition, 1.0);
    OutNormal = (Model * vec4(InNormal, 0.0)).xyz;

    SGPUSurface Surface = Surfaces[InSurfaceIndex];
    OutTexCoord = vec2(dot(InPosition, Surface.VectorX) + Surface.DistanceX,
                       dot(InPosition, Surface.VectorY) + Surface.DistanceY);

    OutTexIndex = InTextureIndex;
}
