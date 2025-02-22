struct SCreep
{
    vec3 Position;
    float Speed;
    vec3 Velocity;
    uint Flags;
    vec3 Color;
    float Padding3;
};

layout(set = 1, binding = 0) readonly buffer UCreep
{
    SCreep Creeps[];
};
