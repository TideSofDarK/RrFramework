#version 460
#extension GL_ARB_shading_language_include : require

layout(location = 0) in vec4 in_color;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_position;
layout(location = 4) in vec3 in_viewPosition;

layout(location = 0) out vec4 out_color;

#include "Uber3D.glsl"

void main()
{
    vec2 uv = in_uv;
    uv.y = 1.0 - uv.y;

    vec3 lightDir = u_globals.directionalLightDirection.xyz;
    vec3 lightColor = u_globals.directionalLightColor.xyz;

    vec3 normal = normalize(in_normal);
    float diffuseAlpha = max(dot(normal, lightDir), 0.0);
    vec3 diffuseColor = lightColor * diffuseAlpha;

    vec3 reflectDir = reflect(-lightDir, normal);

    vec3 viewDir = normalize(in_viewPosition - in_position);
    float specularAlpha = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = 0.5f * specularAlpha * lightColor;
     specular *= 0.0f;

    vec3 color = texture(u_texture[0], uv).rgb * (u_globals.ambientLightColor.xyz + diffuseColor + specular);
    out_color = vec4(color, 1.0f);
}
