#version 460

layout (location = 0) out vec4 FragColor;

layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

layout (set = 2, binding = 0) uniform sampler2D textures[];
layout (set = 2, binding = 1) uniform MaterialUniforms 
{
    vec3 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
};

#define PI 3.141592

void main() 
{
    vec3 albedo = albedoFactor.rgb * pow(texture(textures[0], texCoord).rgb, vec3(2.2));
    vec3 sunDirection = vec3(1, 1, 1);
    vec3 sunColor = vec3(1);
    // vec3 albedo = vec3(1, 1, 1);

    vec3 L = normalize(sunDirection);
    vec3 N = normalize(normal);
    vec3 diffuse = (albedo/PI) * max(dot(N, L), 0);
    FragColor = vec4(diffuse, 1);
}
