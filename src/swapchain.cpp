#include "swapchain.h"

VulkanSwapchain::~VulkanSwapchain()
{
    Shutdown();
}

bool VulkanSwapchain::Init(VulkanDevice *device_, VkSurfaceKHR surface, uint32_t width, uint32_t height)
{
    m_device = device_;

    // Specifically the families which access the swapchain. So potentially different from the device.
    uint32_t graphicsFamilyIndex = m_device->GetGraphicsFamilyIndex();
    uint32_t presentFamilyIndex = m_device->GetPresentFamilyIndex();
    VkDevice device = m_device->GetDevice();

    const uint32_t uniqueFamilyIndexCount = graphicsFamilyIndex == presentFamilyIndex ? 1 : 2;
    const uint32_t uniqueFamilyIndices[] = {graphicsFamilyIndex, presentFamilyIndex};

    m_swapchainExtent.width = width;
    m_swapchainExtent.height = height;
    m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;

    VkSwapchainCreateInfoKHR swapchainCI = {};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = surface;
    swapchainCI.minImageCount = 2;
    swapchainCI.imageFormat = m_swapchainFormat;
    swapchainCI.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainCI.imageExtent = m_swapchainExtent;
    swapchainCI.imageArrayLayers = 1;
    swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.imageSharingMode = uniqueFamilyIndexCount == 1 ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    swapchainCI.queueFamilyIndexCount = uniqueFamilyIndexCount;
    swapchainCI.pQueueFamilyIndices = uniqueFamilyIndices;
    swapchainCI.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchainCI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCI.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainCI.clipped = VK_TRUE;
    swapchainCI.oldSwapchain = nullptr;
    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &m_swapchain));

    vkGetSwapchainImagesKHR(device, m_swapchain, &m_swapchainImageCount, nullptr);
    m_swapchainImages.resize(m_swapchainImageCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &m_swapchainImageCount, &m_swapchainImages[0]);

    m_swapchainImageViews.resize(m_swapchainImageCount);
    for (uint32_t i = 0; i < m_swapchainImageCount; ++i) {
        VkImageViewCreateInfo imageViewCI = {};
        imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCI.image = m_swapchainImages[i];
        imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCI.format = m_swapchainFormat;
        imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCI.subresourceRange.baseMipLevel = 0;
        imageViewCI.subresourceRange.levelCount = 1;
        imageViewCI.subresourceRange.baseArrayLayer = 0;
        imageViewCI.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &imageViewCI, nullptr, &m_swapchainImageViews[i]));
    }

    m_isInitilized = true;
    return true;
}

void VulkanSwapchain::Shutdown()
{
    if (!m_isInitilized) {
        return;
    }
    m_isInitilized = false;

    VkDevice device = m_device->GetDevice();

    while (m_swapchainImageCount--) {
        vkDestroyImageView(device, m_swapchainImageViews.back(), nullptr);
        m_swapchainImageViews.pop_back();
    }
    vkDestroySwapchainKHR(device, m_swapchain, nullptr);
    m_device = nullptr;
}