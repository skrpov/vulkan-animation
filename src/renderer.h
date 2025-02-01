#pragma once

#include "device.h"
#include "scene.h"
#include "swapchain.h"
#include "vulkan.h"

class Renderer
{
  public:
    bool Init(GLFWwindow *window);
    bool RenderUI();
    bool Render(Scene &scene, GLFWwindow *window, double dt);
    void Shutdown();
    bool LoadModel(Scene &scene, const char *path);

  private:
    void RenderNode(VkCommandBuffer commandBuffer, uint32_t frameIndex, Model &model, const Node &node);

    bool ReadFileBytes(const char *path, std::vector<uint8_t> &outBytes);
    bool CompileShader(const void *bytes, uint32_t size, VkShaderModule &outShader);

    bool CreateInstance();
    bool CreateSurface(GLFWwindow *window);
    bool ChoosePhysicalDevice();
    bool CreateDevice();
    bool CreateDepthBuffer();
    void DestroyDepthBuffer();
    bool CreateDescriptorSetLayouts();
    bool CreateFrameData();
    bool CreateDescriptorSets();
    bool CreatePipelineLayouts();
    bool CreateGraphicsPipelines();
    bool InitVulkan(GLFWwindow *window);

    bool HandleResize(GLFWwindow *window);

    bool m_isInitilized = false;

    VkInstance m_instance = nullptr;
    VkSurfaceKHR m_surface = nullptr;
    VulkanDevice m_device;
    VulkanSwapchain m_swapchain;

    VkImage m_depthBufferImage;
    VkImageView m_depthBufferImageView;
    VkFormat m_depthBufferFormat;
    VkDeviceMemory m_depthBufferMemory;

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

    VkPipelineLayout m_pipelineLayout = nullptr;
    VkPipeline m_pipeline = nullptr;
};
