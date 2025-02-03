#version 450

layout(location = 0) out vec2 OutUV;

const vec3 Positions[3] = vec3[3](vec3(-1.0, -1.0, 0.0), vec3(3.0, -1.0, 0.0), vec3(-1.0, 3.0, 0.0));
const vec2 UVs[3] = vec2[3](vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));

void main()
{
    gl_Position = vec4(Positions[gl_InstanceIndex], 1.0f);
    OutUV = UVs[gl_InstanceIndex];
}
