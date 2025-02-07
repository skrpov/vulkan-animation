#include "device.h"

VulkanDevice::~VulkanDevice()
{
    Shutdown();
}

bool VulkanDevice::Init(VkInstance instance, VkSurfaceKHR surface)
{
    m_isInitilized = ChoosePhysicalDevice(instance, surface) && CreateDevice() && CreateCommandPool();
    return m_isInitilized;
}

void VulkanDevice::Shutdown()
{
    if (!m_isInitilized) {
        return;
    }
    m_isInitilized = false;

    vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    vkDestroyDevice(m_device, nullptr);
}

bool VulkanDevice::ChoosePhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
    std::vector<VkPhysicalDevice> physicalDevices;
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    physicalDevices.resize(physicalDeviceCount);
    vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevices[0]);

    for (auto physicalDevice : physicalDevices) {
        uint32_t queueFamilyCount = 0;
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        queueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, &queueFamilyProperties[0]);

        uint32_t graphicsFamilyIndex = UINT32_MAX;
        uint32_t presentFamilyIndex = UINT32_MAX;
        for (uint32_t queueFamilyIndex = 0; queueFamilyIndex < queueFamilyCount; ++queueFamilyIndex) {
            VkBool32 presentSupport;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, surface, &presentSupport);
            if (presentSupport) {
                presentFamilyIndex = queueFamilyIndex;
            }

            if (queueFamilyProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                graphicsFamilyIndex = queueFamilyIndex;
            }
        }

        if (graphicsFamilyIndex != UINT32_MAX && presentFamilyIndex != UINT32_MAX) {

            m_graphicsFamilyIndex = graphicsFamilyIndex;
            m_presentFamilyIndex = presentFamilyIndex;
            m_physicalDevice = physicalDevice;
            vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_memoryProperties);

            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                // If found a integrated gpu that is supported, keep it unless there is a discrete one
                // found later.
                break;
            }
        }
    }

    if (m_physicalDevice == nullptr) {
        LOG_ERROR("Couldn't find suitable graphics card.\n");
        return false;
    }

    return true;
}

bool VulkanDevice::CreateDevice()
{
    const float queuePriority = 1.0f;
    const uint32_t uniqueFamilyIndexCount = m_graphicsFamilyIndex == m_presentFamilyIndex ? 1 : 2;
    const uint32_t uniqueFamilyIndices[] = {m_graphicsFamilyIndex, m_presentFamilyIndex};
    VkDeviceQueueCreateInfo queueInfos[2] = {};
    for (uint32_t i = 0; i < uniqueFamilyIndexCount; ++i) {
        queueInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[i].queueFamilyIndex = uniqueFamilyIndices[i];
        queueInfos[i].queueCount = 1;
        queueInfos[i].pQueuePriorities = &queuePriority;
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = {};
    dynamicRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRendering.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceSynchronization2Features sync2 = {};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.pNext = &dynamicRendering;
    sync2.synchronization2 = VK_TRUE;

    VkPhysicalDeviceFeatures enabledFeatures = {};

    VkDeviceCreateInfo deviceCI = {};
    deviceCI.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCI.pNext = &sync2;
    deviceCI.queueCreateInfoCount = uniqueFamilyIndexCount;
    deviceCI.pQueueCreateInfos = queueInfos;
    deviceCI.enabledLayerCount = ARRAY_COUNT(layers);
    deviceCI.ppEnabledLayerNames = layers;
    deviceCI.enabledExtensionCount = ARRAY_COUNT(deviceExtensions);
    deviceCI.ppEnabledExtensionNames = deviceExtensions;
    deviceCI.pEnabledFeatures = &enabledFeatures;
    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCI, nullptr, &m_device));

    volkLoadDevice(m_device);

    vkGetDeviceQueue(m_device, m_graphicsFamilyIndex, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamilyIndex, 0, &m_presentQueue);

    return true;
}

bool VulkanDevice::AllocateDeviceMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags propertyFlags,
                                        VkDeviceMemory &outMemory)
{
    outMemory = nullptr;
    for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i) {
        uint32_t memoryTypeBits = (1 << i);
        if ((requirements.memoryTypeBits & memoryTypeBits) == memoryTypeBits) {
            if ((m_memoryProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
                VkMemoryAllocateInfo allocateInfo = {};
                allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocateInfo.allocationSize = requirements.size;
                allocateInfo.memoryTypeIndex = i;
                VK_CHECK(vkAllocateMemory(m_device, &allocateInfo, nullptr, &outMemory));
                break;
            }
        }
    }
    if (outMemory == nullptr) {
        return false;
    }

    return true;
}

bool VulkanDevice::CreateBuffer(const BufferCreateInfo &bufferInfo, AllocatedBuffer &outBuffer)
{
    bool hostVisible = bufferInfo.hostVisible;

    VkBufferCreateInfo bufferCI = {};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = bufferInfo.size;
    bufferCI.usage = bufferInfo.usage;
    VK_CHECK(vkCreateBuffer(m_device, &bufferCI, nullptr, &outBuffer.buffer));

    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(m_device, outBuffer.buffer, &requirements);

    const VkMemoryPropertyFlags propertyFlags =
        hostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                    : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (!AllocateDeviceMemory(requirements, propertyFlags, outBuffer.memory)) {
        return false;
    }

    VK_CHECK(vkBindBufferMemory(m_device, outBuffer.buffer, outBuffer.memory, 0));
    outBuffer.size = bufferInfo.size;

    if (hostVisible) {
        VK_CHECK(vkMapMemory(m_device, outBuffer.memory, 0, outBuffer.size, 0, &outBuffer.data));
    }

    return true;
}

bool VulkanDevice::CreateCommandPool()
{
    VkCommandPoolCreateInfo commandPoolCI = {};
    commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCI.queueFamilyIndex = m_graphicsFamilyIndex;
    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCI, nullptr, &m_commandPool));

    return true;
}

bool VulkanDevice::SetBufferData(AllocatedBuffer &buffer, VkDeviceSize offset, VkDeviceSize size, const void *data)
{
    assert(size > 0);
    assert(data != nullptr);
    assert(offset + size <= buffer.size);
    if (buffer.data != nullptr) {
        return SetBufferData_Direct(buffer, offset, size, data);
    }
    return SetBufferData_Staged(buffer, offset, size, data);
}

bool VulkanDevice::SetBufferData_Direct(AllocatedBuffer &buffer, VkDeviceSize offset, VkDeviceSize size,
                                        const void *data)
{
    uint8_t *copyDest = ((uint8_t *)buffer.data) + offset;
    memcpy(copyDest, data, size);
    return true;
}

bool VulkanDevice::SetBufferData_Staged(AllocatedBuffer &buffer, VkDeviceSize offset, VkDeviceSize size,
                                        const void *data)
{
    AllocatedBuffer &tempBuffer = m_tempBuffers.emplace_back();
    VkFence &fence = m_fences.emplace_back();
    VkCommandBuffer &commandBuffer = m_commandBuffers.emplace_back();

    BufferCreateInfo bufferInfo = {};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.hostVisible = true;
    if (!CreateBuffer(bufferInfo, tempBuffer) || !SetBufferData(tempBuffer, offset, size, data)) {
        return false;
    }

    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = GetCommandPool();
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(GetDevice(), &allocateInfo, &commandBuffer));

    VkFenceCreateInfo fenceCI = {};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(GetDevice(), &fenceCI, nullptr, &fence));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    {
        VkBufferCopy region = {};
        region.srcOffset = 0;
        region.dstOffset = offset;
        region.size = size;
        vkCmdCopyBuffer(commandBuffer, tempBuffer.buffer, buffer.buffer, 1, &region);
    }
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(GetGraphicsQueue(), 1, &submitInfo, fence));

    return true;
}

void VulkanDevice::DestroyBuffer(AllocatedBuffer &buffer)
{
    vkFreeMemory(GetDevice(), buffer.memory, nullptr);
    vkDestroyBuffer(GetDevice(), buffer.buffer, nullptr);
}

bool VulkanDevice::WaitForTransfers()
{
    if (!m_fences.empty()) {
        VK_CHECK(vkDeviceWaitIdle(m_device)); // HACK:
        VK_CHECK(vkWaitForFences(GetDevice(), m_fences.size(), &m_fences[0], VK_TRUE, ~0ull));
        while (!m_tempBuffers.empty()) {
            DestroyBuffer(m_tempBuffers.back());
            vkDestroyFence(GetDevice(), m_fences.back(), nullptr);
            vkFreeCommandBuffers(GetDevice(), GetCommandPool(), 1, &m_commandBuffers.back());
            m_commandBuffers.pop_back();
            m_tempBuffers.pop_back();
            m_fences.pop_back();
        }
    }

    return true;
}

bool VulkanDevice::CreateImage(const ImageCreateInfo &imageInfo, AllocatedImage &outImage)
{
    assert(imageInfo.extent.width > 0);
    assert(imageInfo.extent.height > 0);
    assert(imageInfo.extent.depth > 0);

    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = imageInfo.format;
    imageCI.extent = imageInfo.extent;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.usage = imageInfo.usage;

    VK_CHECK(vkCreateImage(GetDevice(), &imageCI, nullptr, &outImage.image));

    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(GetDevice(), outImage.image, &requirements);

    const VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (!AllocateDeviceMemory(requirements, propertyFlags, outImage.memory)) {
        return false;
    }

    VK_CHECK(vkBindImageMemory(GetDevice(), outImage.image, outImage.memory, 0));

    VkImageViewCreateInfo imageViewCI = {};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.image = outImage.image;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = imageInfo.format;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    
    VK_CHECK(vkCreateImageView(GetDevice(), &imageViewCI, nullptr, &outImage.imageView));

    outImage.format = imageInfo.format;
    outImage.extent = imageInfo.extent;
    
    return true;
}

void VulkanDevice::DestroyImage(AllocatedImage &image) 
{
    vkDestroyImageView(GetDevice(), image.imageView, nullptr);
    vkFreeMemory(GetDevice(), image.memory, nullptr);
    vkDestroyImage(GetDevice(), image.image, nullptr);
}

bool VulkanDevice::SetImageData(AllocatedImage &image, const void *data)
{
    AllocatedBuffer &tempBuffer = m_tempBuffers.emplace_back();
    VkFence &fence = m_fences.emplace_back();
    VkCommandBuffer &commandBuffer = m_commandBuffers.emplace_back();

    VkDeviceSize size = image.GetSize();
    assert(image.extent.depth == 1 && "Multi-layered images are not handled yet.");

    BufferCreateInfo bufferInfo = {};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.hostVisible = true;
    if (!CreateBuffer(bufferInfo, tempBuffer)) {
        return false;
    }
    if (!SetBufferData(tempBuffer, 0, size, data)) {
        return false;
    }

    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = GetCommandPool();
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(GetDevice(), &allocateInfo, &commandBuffer));

    VkFenceCreateInfo fenceCI = {};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(GetDevice(), &fenceCI, nullptr, &fence));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    {
        TransitionImageLayout(commandBuffer, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = image.extent.width;
        region.bufferImageHeight = image.extent.height;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {};
        region.imageExtent = image.extent;

        vkCmdCopyBufferToImage(commandBuffer, tempBuffer.buffer, image.image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    }
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(GetGraphicsQueue(), 1, &submitInfo, fence));

    return true;
}


































