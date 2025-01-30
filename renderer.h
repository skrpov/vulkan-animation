#pragma once

#include <volk.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <stdio.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>

#define MAX_FRAMES_IN_FLIGHT 2
#define ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))
#define VK_CHECK(call)\
do {\
    VkResult res = call;\
    if (res != VK_SUCCESS) {\
        fprintf(stderr, "ERROR: %s - %s\n", #call, string_VkResult(res));\
        return false;\
    }\
} while (0)

static const char *layers[] = {
    "VK_LAYER_KHRONOS_validation",
};

static const char *instanceExtensions[] = {
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
    "VK_KHR_portability_enumeration",
    "VK_EXT_debug_utils",
    "VK_EXT_metal_surface",
    "VK_KHR_surface",
};

static const char *deviceExtensions[] = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    "VK_KHR_portability_subset",
    "VK_KHR_swapchain",
    VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
};

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
    glm::vec3 target = glm::vec3(0, 0, 0);
    float fov = glm::radians(45.0f);
    float near = 0.1f;
    float far = 100.0f;
    bool flipY = true;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceSize size;
    void *data;
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
        return 
            glm::translate(glm::mat4(1), translation) *
            glm::scale(glm::mat4(1), scale) * 
            glm::mat4_cast(rotation) * 
            matrix;
    }
};

enum InterpolationMethod 
{
    InterpolationMethod_Linear = 0,
};

template <typename T>
struct AnimationSpline
{
    void GetValueAtTime(float time, T &outValue) const;

    std::vector<T> values;
    std::vector<float> times;
    InterpolationMethod method;
};

template <typename T>
inline void AnimationSpline<T>::GetValueAtTime(float time, T &outValue) const
{
    assert(method == InterpolationMethod_Linear && "Unhandled interpolation method");

    for (uint32_t i = 1; i < values.size(); ++i) {
        auto v0 = values[i-1];
        auto v1 = values[i];
        auto t0 = times[i-1];
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
    std::vector<Node *> joints; // readonly
    std::vector<glm::mat4> jointMatrices[MAX_FRAMES_IN_FLIGHT];
    AllocatedBuffer jointMatricesBuffer[MAX_FRAMES_IN_FLIGHT];
    VkDescriptorSet descriptorSet[MAX_FRAMES_IN_FLIGHT];
};

struct Model 
{
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
    float  animation_t = 0.0f;
    Animation *playingAnimation = nullptr;
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
};

class Renderer 
{
public:
    bool Init(GLFWwindow *window);
    bool Render(const Camera &camera, GLFWwindow *window, double dt);
    void Shutdown();
    bool LoadModel(const char *path);

private:
    void RenderNode(VkCommandBuffer commandBuffer, uint32_t frameIndex, Model &model, const Node &node);

    bool ReadFileBytes(const char *path, std::vector<uint8_t> &outBytes);
    bool CompileShader(const void *bytes, uint32_t size, VkShaderModule &outShader);

    bool AllocateDeviceMemory(VkMemoryRequirements requirements,VkMemoryPropertyFlags propertyFlags, VkDeviceMemory &outMemory);
    bool CreateBuffer(VkBufferUsageFlags usage, VkDeviceSize size, AllocatedBuffer &outBuffer);
    bool CreateImage();

    bool CreateInstance();
    bool CreateSurface(GLFWwindow *window);
    bool ChoosePhysicalDevice();
    bool CreateDevice();
    bool CreateSwapchain(GLFWwindow *window);
    bool CreateDepthBuffer();
    void DestroyDepthBuffer();
    void DestroySwapchain();
    bool CreateDescriptorSetLayouts();
    bool CreateFrameData();
    bool CreateDescriptorSets();
    bool CreatePipelineLayouts();
    bool CreateGraphicsPipelines();
    bool InitVulkan(GLFWwindow *window);

    bool HandleResize(GLFWwindow *window);

    VkInstance m_instance = nullptr;
    VkSurfaceKHR m_surface = nullptr;
    VkPhysicalDevice m_physicalDevice = nullptr;
    uint32_t m_graphicsFamilyIndex = UINT32_MAX;
    uint32_t m_presentFamilyIndex = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    VkDevice m_device = nullptr;
    VkQueue m_graphicsQueue = nullptr;
    VkQueue m_presentQueue = nullptr;

    uint32_t m_swapchainImageCount = 0;
    VkSwapchainKHR m_swapchain = nullptr;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainFormat;
    VkExtent2D m_swapchainExtent;

    VkImage m_depthBufferImage;
    VkImageView m_depthBufferImageView;
    VkFormat m_depthBufferFormat;
    VkDeviceMemory m_depthBufferMemory;

    VkCommandPool m_commandPool = nullptr;
    uint32_t m_nextFrameIndex = 0;
    VkFence m_commandBufferReady[MAX_FRAMES_IN_FLIGHT] = {};
    VkCommandBuffer m_commandBuffers[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore m_imageReady[MAX_FRAMES_IN_FLIGHT] = {};
    VkSemaphore m_renderFinished[MAX_FRAMES_IN_FLIGHT] = {};

    VkDescriptorPool m_descriptorPool; 
    VkDescriptorSetLayout m_jointsDescriptorsLayout;
    VkDescriptorSetLayout m_globalDescriptorsLayout;
    AllocatedBuffer m_globalUniformBuffers[MAX_FRAMES_IN_FLIGHT] = {};
    VkDescriptorSet m_globalDescriptors[MAX_FRAMES_IN_FLIGHT] = {};

    AllocatedBuffer m_vertexBuffer = {};
    VkPipelineLayout m_pipelineLayout = nullptr;
    VkPipeline m_pipeline = nullptr;

    // TODO: Scene.
    std::vector<Model> m_models;
};

