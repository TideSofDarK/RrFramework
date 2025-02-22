#version 450
#extension GL_ARB_shading_language_include : require

#include "Creep.glsl"

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 InUV;
layout(location = 2) in vec3 InNormal;

layout(location = 0) out vec2 OutUV;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec3 OutViewDir;
layout(location = 3) out uint OutIndex;

layout(set = 0, binding = 0) uniform Globals
{
    mat4 Model;
    mat4 View;
    mat4 Projection;
};

void main()
{
    vec3 Dir = -normalize(Creeps[gl_InstanceIndex].Velocity);
    vec3 XAxis = -normalize(cross(vec3(0.0f, 1.0f, 0.0f), Dir));
    vec3 YAxis = normalize(cross(Dir,XAxis));
    mat4 CreepModel = mat4(1.0f);
    CreepModel[0][0] = XAxis.x;
    CreepModel[0][1] = YAxis.x;
    CreepModel[0][2] = Dir.x;
    CreepModel[1][0] = XAxis.y;
    CreepModel[1][1] = YAxis.y;
    CreepModel[1][2] = Dir.y;
    CreepModel[2][0] = XAxis.z;
    CreepModel[2][1] = YAxis.z;
    CreepModel[2][2] = Dir.z;
    CreepModel[3] = vec4(Creeps[gl_InstanceIndex].Position, 1.0f);
    gl_Position = Projection * View * CreepModel * vec4(InPosition, 1.0f);
    OutNormal = mat3(transpose(inverse(CreepModel))) * InNormal;
    OutUV = InUV;
    OutViewDir = mat3(View) * vec3(0.0f, 0.0f, 1.0f);
    OutIndex = gl_InstanceIndex;
}
