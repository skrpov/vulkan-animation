#pragma once
#include "vulkan.h"

class Buffer;
struct AllocatedBuffer;
class VulkanDevice;

struct ImageCreateInfo 
{
    VkFormat format;
    VkExtent3D extent = {};
    VkImageUsageFlags usage;
};

struct AllocatedImage 
{
    VkImage image = nullptr;
    VkImageView imageView = nullptr;
    VkDeviceMemory memory = nullptr;
    VkFormat format;
    VkExtent3D extent = {};

    inline VkDeviceSize GetSize() const { return GetFormatSize(format)*extent.width*extent.height*extent.depth; }
};

struct BufferCreateInfo
{
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bool hostVisible = true;
};

struct AllocatedBuffer
{
    uint32_t rc = 0;
    VkBuffer buffer = nullptr;
    VkDeviceMemory memory = nullptr;
    VkDeviceSize size = 0;
    void *data = nullptr;
};

class VulkanDevice
{
  public:
    VulkanDevice() = default;
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice &) = delete;
    VulkanDevice(VulkanDevice &&) = delete;
    VulkanDevice &operator=(const VulkanDevice &) = delete;
    VulkanDevice &operator=(VulkanDevice &&) = delete;

    bool Init(VkInstance instance, VkSurfaceKHR surface);
    void Shutdown();

    inline VkDevice GetDevice() const { return m_device; }
    inline VkCommandPool GetCommandPool() const { return m_commandPool; }
    inline VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    inline VkQueue GetPresentQueue() const { return m_presentQueue; }
    inline uint32_t GetGraphicsFamilyIndex() const { return m_graphicsFamilyIndex; }
    inline uint32_t GetPresentFamilyIndex() const { return m_presentFamilyIndex; }

    bool AllocateDeviceMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags propertyFlags,
                              VkDeviceMemory &outMemory);

    bool CreateBuffer(const BufferCreateInfo &bufferInfo, AllocatedBuffer &outBuffer);
    void DestroyBuffer(AllocatedBuffer &buffer);
    bool SetBufferData(AllocatedBuffer &buffer, VkDeviceSize offset, VkDeviceSize size, const void *data);
    bool WaitForTransfers();

    bool CreateImage(const ImageCreateInfo &imageInfo, AllocatedImage &outImage);
    void DestroyImage(AllocatedImage &image);
    bool SetImageData(AllocatedImage &image, const void *data);

  private:
    bool ChoosePhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    bool CreateDevice();
    bool CreateCommandPool();

    bool SetBufferData_Staged(AllocatedBuffer &buffer, VkDeviceSize offset, VkDeviceSize size, const void *data);
    bool SetBufferData_Direct(AllocatedBuffer &buffer, VkDeviceSize offset, VkDeviceSize size, const void *data);

    bool m_isInitilized = false;
    VkPhysicalDevice m_physicalDevice = nullptr;
    uint32_t m_graphicsFamilyIndex = UINT32_MAX;
    uint32_t m_presentFamilyIndex = UINT32_MAX;
    VkPhysicalDeviceMemoryProperties m_memoryProperties = {};

    VkDevice m_device = nullptr;
    VkCommandPool m_commandPool = nullptr;
    VkQueue m_graphicsQueue = nullptr;
    VkQueue m_presentQueue = nullptr;

    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkFence> m_fences;
    std::vector<AllocatedBuffer> m_tempBuffers;
};
