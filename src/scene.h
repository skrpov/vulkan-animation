#pragma once

#include "device.h"

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::ivec4 joints;
    glm::vec4 weights;
};

struct Constants
{
    glm::mat4 model;
};

struct GlobalUniforms
{
    glm::mat4 viewProjection;
};

struct Camera
{
    glm::vec3 position = glm::vec3(0, 0, 4);
    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 target = glm::vec3(0, 0.7, 0);
    float fov = glm::radians(45.0f);
    float near = 0.1f;
    float far = 100.0f;
    bool flipY = true;
};

struct Primitive
{
    uint32_t indexOffset;
    uint32_t indexCount;
};

struct Mesh
{
    uint32_t primitiveOffset;
    uint32_t primitiveCount;
};

struct Node
{
    glm::vec3 translation = glm::vec3(0);
    glm::vec3 scale = glm::vec3(1);
    glm::quat rotation = glm::quat();
    glm::mat4 matrix = glm::mat4(1);

    glm::mat4 worldMatrix;

    uint32_t skinIndex = UINT32_MAX;
    uint32_t nodeIndex = UINT32_MAX;
    uint32_t meshIndex = UINT32_MAX;
    std::vector<Node> children;

    inline glm::mat4 GetLocalMatrix() const
    {
        return glm::translate(glm::mat4(1), translation) * glm::scale(glm::mat4(1), scale) * glm::mat4_cast(rotation) *
               matrix;
    }
};

enum InterpolationMethod
{
    InterpolationMethod_Linear = 0,
};

template <typename T> struct AnimationSpline
{
    void GetValueAtTime(float time, T &outValue) const;

    std::vector<T> values;
    std::vector<float> times;
    InterpolationMethod method;
};

template <> inline void AnimationSpline<glm::quat>::GetValueAtTime(float time, glm::quat &outValue) const
{
    assert(method == InterpolationMethod_Linear && "Unhandled interpolation method");

    for (uint32_t i = 1; i < values.size(); ++i) {
        auto v0 = values[i - 1];
        auto v1 = values[i];
        auto t0 = times[i - 1];
        auto t1 = times[i];

        if (t1 > time) {
            float t = (time - t0) / (t1 - t0);
            outValue = glm::normalize(glm::slerp(v0, v1, t));
            break;
        }
    }
}

template <typename T> inline void AnimationSpline<T>::GetValueAtTime(float time, T &outValue) const
{
    assert(method == InterpolationMethod_Linear && "Unhandled interpolation method");

    for (uint32_t i = 1; i < values.size(); ++i) {
        auto v0 = values[i - 1];
        auto v1 = values[i];
        auto t0 = times[i - 1];
        auto t1 = times[i];

        if (t1 > time) {
            float t = (time - t0) / (t1 - t0);
            outValue = glm::mix(v0, v1, t);
            break;
        }
    }
}

struct AnimationSampler
{
    // For the time being assume that animations share a model's lifetime
    // and therefore this pointer will always be valid.
    Node *node;

    AnimationSpline<glm::vec3> scale;
    AnimationSpline<glm::vec3> translation;
    AnimationSpline<glm::quat> rotation;
};

// Since animations work on specific node they cannot be shared between models,
// so even thougth they create a cyclical dependency it makes sense.
struct Animation
{
    std::vector<AnimationSampler> samplers;
    float endTime;
};

struct Skin
{
    std::vector<glm::mat4> inverseBindMatrices; // readonly
    std::vector<Node *> joints;                 // readonly
    std::vector<glm::mat4> jointMatrices[MAX_FRAMES_IN_FLIGHT];
    AllocatedBuffer jointMatricesBuffer[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet descriptorSet[MAX_FRAMES_IN_FLIGHT];
};

struct Model
{
    inline void SetPlayingAnimation(uint32_t index)
    {
        playingAnimation = &animations[index];
        animation_t = 0.0;
    }

    void UpdateAnimations(float dt);
    void UpdateTransforms();
    void UpdateTransforms(glm::mat4 parentMatrix, Node &node);

    // Static
    std::vector<Mesh> meshes;
    std::vector<Primitive> primitives;
    std::vector<Animation> animations;
    std::vector<Skin> skins;

    // TODO:
    //
    // If I have many instances of a model some of this information will
    // remain constant (vertex data, etc) some of it however changes at runtime (ie node positions)
    // so there is probobly two seperate things here.

    // Dynamic
    Node rootNode;
    float animation_t = 0.0f;
    Animation *playingAnimation = nullptr;
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
};

struct Scene
{
    std::vector<Model> models;
    Camera camera;
    glm::vec3 sunDirection;
};
