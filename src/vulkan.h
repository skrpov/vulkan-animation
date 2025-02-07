#pragma once

#include <volk.h>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdio.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>

#define LOG_ERROR(message, ...) fprintf(stderr, "ERROR: " message "\n", ##__VA_ARGS__)

#define MAX_FRAMES_IN_FLIGHT 3
#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))
#define VK_CHECK(call)                                                                                                 \
    do {                                                                                                               \
        VkResult res = call;                                                                                           \
        if (res != VK_SUCCESS) {                                                                                       \
            LOG_ERROR("%s - %s", #call, string_VkResult(res));                                                         \
            return false;                                                                                              \
        }                                                                                                              \
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

inline VkDeviceSize GetFormatSize(VkFormat format)
{
    switch (format) 
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return sizeof(float)*4;
    case VK_FORMAT_R32G32B32A32_UINT:
        return sizeof(uint32_t)*4;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return sizeof(uint32_t);
    }

    assert(false && "Unhandled image format");
}

inline void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                                  VkImageLayout newLayout, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
{
    VkImageMemoryBarrier2 imageBarrier = {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
    imageBarrier.oldLayout = oldLayout;
    imageBarrier.newLayout = newLayout;
    imageBarrier.image = image;
    imageBarrier.subresourceRange.aspectMask = aspect;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dependencyInfo = {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2KHR(commandBuffer, &dependencyInfo);
}


