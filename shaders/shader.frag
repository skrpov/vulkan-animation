#version 460

#define PI 3.141592

layout (location = 0) out vec4 FragColor;

layout (location = 0) in vec3 worldPosition;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 texCoord;

layout(std140, set = 0, binding = 0) uniform GlobalUniforms 
{
    mat4 viewProjection;
    vec3 cameraPosition;
};

layout (set = 2, binding = 0) uniform sampler2D textures[];
layout (set = 2, binding = 1) uniform MaterialUniforms 
{
    vec4 albedoFactor;
    float metallicFactor;
    float roughnessFactor;
};

vec3 Fresnel_Schlick(vec3 F0, float VdotH) 
{
    return F0 + (1 - F0) * pow(1 - VdotH, 5);
}

float Distrobution_Blinn(float roughness, float NdotH) 
{
    float a = roughness * roughness;
    float a_2 = a * a;
    float power = 2/a_2 - 2;
    return pow(NdotH, power) / (PI * a_2);
}

float Geometry_CookTorrance(float NdotH, float NdotV, float NdotL, float VdotH) 
{
    return min(1, min(2*NdotH*NdotV/VdotH, 2*NdotH*NdotL/VdotH));
}

void main() 
{
    vec3 albedo = albedoFactor.rgb * pow(texture(textures[0], texCoord).rgb, vec3(2.2));
    float roughness = roughnessFactor;
    float metallic = metallicFactor;

    vec3 sunDirection = vec3(1, 1, 1);
    vec3 sunColor = vec3(1);

    vec3 V = normalize(cameraPosition - worldPosition);
    vec3 L = normalize(sunDirection);
    vec3 N = normalize(normal);
    vec3 H = normalize(L + V);

    float NdotV = max(dot(N, V), 0.0001f);
    float NdotH = max(dot(N, H), 0.0001f);
    float NdotL = max(dot(N, L), 0.0001f);
    float VdotH = max(dot(V, H), 0.0001f);

    vec3 F0 = mix(vec3(0.04), albedo, vec3(metallic));

    vec3 F = Fresnel_Schlick(F0, VdotH);
    float D = Distrobution_Blinn(roughness, NdotH);
    float G = Geometry_CookTorrance(NdotH, NdotV, NdotL, VdotH);

    vec3 s = F;
    vec3 d = (1 - s) * (1.0f - metallic);

    vec3 rs = F * D * G / (4*NdotL*NdotV);
    vec3 rd = (albedo/PI);

    vec3 color = sunColor * NdotL * (d * rd + rs);

    FragColor = vec4(color, 1);
}
