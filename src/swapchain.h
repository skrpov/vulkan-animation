#pragma once

#include "device.h"

class VulkanSwapchain 
{
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain();

    VulkanSwapchain(const VulkanSwapchain &) = delete;
    VulkanSwapchain(VulkanSwapchain &&) = delete;
    VulkanSwapchain &operator=(const VulkanSwapchain &) = delete;
    VulkanSwapchain &operator=(VulkanSwapchain &&) = delete;

    bool Init(VulkanDevice *device, VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void Shutdown();

    inline VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    inline VkImage GetImage(uint32_t index) const { return m_swapchainImages[index]; }
    inline VkImageView GetImageView(uint32_t index) const { return m_swapchainImageViews[index]; }
    inline VkFormat GetFormat() const { return m_swapchainFormat; }
    inline VkExtent2D GetExtent() const { return m_swapchainExtent; }

private:
    bool m_isInitilized = false;
    VulkanDevice *m_device = nullptr;

    uint32_t m_swapchainImageCount = 0;
    VkSwapchainKHR m_swapchain = nullptr;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainFormat;
    VkExtent2D m_swapchainExtent;
};
