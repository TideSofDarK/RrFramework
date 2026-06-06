#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec3 InNormal;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec3 OutPosition;
layout(location = 3) out vec3 OutNormalVS;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 View;
    mat4 Projection;
    vec3 CameraPosition;
    float Time;
    vec2 Resolution;
    vec2 Padding0;
    vec4 AmbientColor;
};

layout(set = 2, binding = 0) readonly buffer SGPUStorage
{
    mat4 ModelArray[];
};

void main()
{
    mat4 Model = ModelArray[gl_InstanceIndex];
    gl_Position = Projection * View * Model * vec4(InPosition, 1.0f);
    OutNormal = mat3(transpose(inverse(Model))) * InNormal;
    OutNormalVS = mat3(transpose(inverse(View * Model))) * InNormal;
    OutUV = InUV;
    OutPosition = (Model * vec4(InPosition, 1.0f)).xyz;
}
