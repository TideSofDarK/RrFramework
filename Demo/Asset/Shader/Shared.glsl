#define RR_UBER3D_SET_GLOBALS 0

struct Vertex {
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
    vec4 color;
};

vec3 processNormal(vec3 normal, mat4 model)
{
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    return normalMatrix * normal;
}
