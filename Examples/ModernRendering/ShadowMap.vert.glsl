#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec3 InNormal;

layout(location = 0) out vec3 OutPosition;

layout(set = 0, binding = 0) uniform SGPUUniform
{
    mat4 ViewProjection;
    vec3 LightPosition;
    float FarPlane;
};

layout(set = 1, binding = 0) readonly buffer SGPUStorage
{
    mat4 ModelArray[];
};

void main()
{
    mat4 Model = ModelArray[gl_InstanceIndex];
    OutPosition = (Model * vec4(InPosition.xyz, 1.0)).xyz;
    gl_Position = ViewProjection * Model * vec4(InPosition.xyz, 1.0);
    // gl_Position.x *= -1.0;
    // gl_Position.y *= -1.0;
}
