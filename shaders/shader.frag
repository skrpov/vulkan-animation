#version 460

layout (location = 0) in vec3 normal;
layout (location = 0) out vec4 FragColor;

#define PI 3.141592

void main() 
{
    vec3 sunDirection = vec3(1, 1, 1);
    vec3 sunColor = vec3(1);
    vec3 albedo = vec3(1, 1, 1);

    vec3 L = normalize(sunDirection);
    vec3 N = normalize(normal);
    vec3 diffuse = (albedo/PI) * max(dot(N, L), 0);
    FragColor = vec4(diffuse, 1);
}
