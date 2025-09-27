#version 460

layout(location = 0) in vec2 InUV;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec3 InPosition;
layout(location = 3) in vec3 InNormalVS;

layout(location = 0) out vec4 OutNormalDepth;

void main()
{
    OutNormalDepth = vec4(normalize(InNormalVS), gl_FragCoord.z);
}
