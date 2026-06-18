#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec3 InNormal;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out uint OutInstanceIndex;

layout(set = 0, binding = 0) uniform UGlobals
{
    mat4 View;
    mat4 Projection;
    float Time;
};

struct SGPUModel
{
    mat4 Model;
    vec4 Color;
};

layout(set = 1, binding = 0) readonly buffer UModels
{
    SGPUModel Models[];
};

void main()
{
    mat4 Model = Models[gl_InstanceIndex].Model;
    gl_Position = Projection * View * Model * vec4(InPosition, 1.0f);
    OutNormal = mat3(transpose(inverse(Model))) * InNormal;
    OutInstanceIndex = gl_InstanceIndex;
    OutUV = InUV;
}
