#version 460 core

layout(local_size_x = 32, local_size_y = 32) in;

layout(rgba8, set = 0, binding = 0) writeonly uniform image2D Image;

void main() {
    ivec2 Coord = ivec2(gl_GlobalInvocationID.xy);
    vec3 Color = vec3(vec2(Coord) / vec2(256.0), 0.0);
    imageStore(Image, Coord, vec4(Color, 1.0));
}
