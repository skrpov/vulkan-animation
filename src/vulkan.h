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
