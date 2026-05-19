#version 460

layout (location = 0) in vec3  position;
layout (location = 1) in vec3  normal;
layout (location = 2) in vec2  texCoord;
layout (location = 3) in ivec4 joints;
layout (location = 4) in vec4  weights;

layout (location = 0) out vec3 outWorldPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outTexCoord;

layout (push_constant) uniform Constants 
{
    mat4 model;
};

layout(std140, set = 0, binding = 0) uniform GlobalUniforms 
{
    mat4 viewProjection;
    vec3 cameraPosition;
};

layout(std140, set = 1, binding = 0) buffer JointUniforms
{
    mat4 jointMatrices[];
};

void main() 
{
    mat4 skinMatrix = 
        weights[0] * jointMatrices[joints[0]] +
        weights[1] * jointMatrices[joints[1]] +
        weights[2] * jointMatrices[joints[2]] +
        weights[3] * jointMatrices[joints[3]];

    outWorldPosition = vec3(model * skinMatrix * vec4(position, 1));
    outNormal = transpose(inverse(mat3(model * skinMatrix))) * normalize(normal);
    outTexCoord = texCoord;
    gl_Position = viewProjection * model * skinMatrix * vec4(position, 1);
}
