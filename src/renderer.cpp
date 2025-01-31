#include "renderer.h"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

bool Renderer::CreateInstance()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instanceCI = {};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    instanceCI.pApplicationInfo = &appInfo;
    instanceCI.enabledLayerCount = ARRAY_COUNT(layers);
    instanceCI.ppEnabledLayerNames = layers;
    instanceCI.enabledExtensionCount = ARRAY_COUNT(instanceExtensions);
    instanceCI.ppEnabledExtensionNames = instanceExtensions;
    VK_CHECK(vkCreateInstance(&instanceCI, nullptr, &m_instance));

    volkLoadInstance(m_instance);

    return true;
}

bool Renderer::CreateSurface(GLFWwindow *window)
{
    VK_CHECK(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface));
    return true;
}

bool Renderer::ChoosePhysicalDevice()
{
    std::vector<VkPhysicalDevice> physicalDevices;
    uint32_t physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);
    physicalDevices.resize(physicalDeviceCount);
    vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, &physicalDevices[0]);

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
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamilyIndex, m_surface, &presentSupport);
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
        return false;
    }

    return true;
}

bool Renderer::CreateDevice()
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

bool Renderer::CreateSwapchain(GLFWwindow *window)
{
    // Specifically the families which access the swapchain. So potentially different from the device.
    const uint32_t uniqueFamilyIndexCount = m_graphicsFamilyIndex == m_presentFamilyIndex ? 1 : 2;
    const uint32_t uniqueFamilyIndices[] = {m_graphicsFamilyIndex, m_presentFamilyIndex};

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    m_swapchainExtent.width = (uint32_t)width;
    m_swapchainExtent.height = (uint32_t)height;
    m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;

    VkSwapchainCreateInfoKHR swapchainCI = {};
    swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCI.surface = m_surface;
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
    VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCI, nullptr, &m_swapchain));

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, nullptr);
    m_swapchainImages.resize(m_swapchainImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchainImageCount, &m_swapchainImages[0]);

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
        VK_CHECK(vkCreateImageView(m_device, &imageViewCI, nullptr, &m_swapchainImageViews[i]));
    }

    return true;
}

bool Renderer::CreateDescriptorSetLayouts()
{
    const VkDescriptorPoolSize poolSizes[] = {
        // type; descriptorCount;
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4096},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096},
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI = {};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.maxSets = 4096;
    descriptorPoolCI.poolSizeCount = ARRAY_COUNT(poolSizes);
    descriptorPoolCI.pPoolSizes = poolSizes;
    VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCI, nullptr, &m_descriptorPool));

    { // Global descriptors

        const VkDescriptorSetLayoutBinding bindings[] = {
            // binding; descriptorType; descriptorCount; stageFlags; pImmutableSamplers;
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo layoutCI = {};
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.bindingCount = ARRAY_COUNT(bindings);
        layoutCI.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutCI, nullptr, &m_globalDescriptorsLayout));
    }

    { // Joints descriptors
        const VkDescriptorSetLayoutBinding bindings[] = {
            // binding; descriptorType; descriptorCount; stageFlags; pImmutableSamplers;
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo layoutCI = {};
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.bindingCount = ARRAY_COUNT(bindings);
        layoutCI.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(m_device, &layoutCI, nullptr, &m_jointsDescriptorsLayout));
    }

    return true;
}

bool Renderer::CreateFrameData()
{
    VkCommandPoolCreateInfo commandPoolCI = {};
    commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCI.queueFamilyIndex = m_graphicsFamilyIndex;
    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCI, nullptr, &m_commandPool));

    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = m_commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        VK_CHECK(vkAllocateCommandBuffers(m_device, &allocateInfo, m_commandBuffers));
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFenceCreateInfo fenceCI = {};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreCI = {};
        semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateFence(m_device, &fenceCI, nullptr, &m_commandBufferReady[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCI, nullptr, &m_imageReady[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCI, nullptr, &m_renderFinished[i]));
    }

    {
        VkDescriptorBufferInfo bufferInfos[MAX_FRAMES_IN_FLIGHT] = {};
        VkWriteDescriptorSet bufferWrites[MAX_FRAMES_IN_FLIGHT] = {};
        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {};
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            layouts[i] = m_globalDescriptorsLayout;
        }

        VkDescriptorSetAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_descriptorPool;
        allocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocateInfo.pSetLayouts = &layouts[0];
        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocateInfo, &m_globalDescriptors[0]));

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (!CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(GlobalUniforms), m_globalUniformBuffers[i])) {
                return false;
            }

            bufferInfos[i].buffer = m_globalUniformBuffers[i].buffer;
            bufferInfos[i].offset = 0;
            bufferInfos[i].range = m_globalUniformBuffers[i].size;

            bufferWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            bufferWrites[i].dstSet = m_globalDescriptors[i];
            bufferWrites[i].dstBinding = 0;
            bufferWrites[i].dstArrayElement = 0;
            bufferWrites[i].descriptorCount = 1;
            bufferWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bufferWrites[i].pBufferInfo = &bufferInfos[i];
        }

        vkUpdateDescriptorSets(m_device, MAX_FRAMES_IN_FLIGHT, bufferWrites, 0, nullptr);
    }

    return true;
}

bool Renderer::CreatePipelineLayouts()
{
    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset = 0;
    range.size = sizeof(Constants);

    const VkDescriptorSetLayout layouts[] = {m_globalDescriptorsLayout, m_jointsDescriptorsLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = ARRAY_COUNT(layouts);
    pipelineLayoutCI.pSetLayouts = layouts;
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &range;
    VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCI, nullptr, &m_pipelineLayout));

    return true;
}

bool Renderer::ReadFileBytes(const char *path, std::vector<uint8_t> &outBytes)
{
    bool result = true;
    FILE *fp = fopen(path, "rb");
    if (fp) {
        fseek(fp, 0L, SEEK_END);
        uint32_t fileSize = (uint32_t)ftell(fp);
        rewind(fp);

        outBytes.resize(fileSize);
        if (fread(&outBytes[0], fileSize, 1, fp) != 1)
            result = false;
        fclose(fp);
    } else {
        result = false;
    }

    return result;
}

bool Renderer::CompileShader(const void *bytes, uint32_t size, VkShaderModule &outShader)
{
    VkShaderModuleCreateInfo shaderCI = {};
    shaderCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCI.codeSize = size;
    shaderCI.pCode = (const uint32_t *)bytes;
    VK_CHECK(vkCreateShaderModule(m_device, &shaderCI, nullptr, &outShader));
    return true;
}

bool Renderer::CreateGraphicsPipelines()
{
    VkShaderModule vertexShader = nullptr;
    VkShaderModule fragmentShader = nullptr;
    std::vector<uint8_t> bytes;

    if (!ReadFileBytes("./shaders/shader.vert.spv", bytes) || !CompileShader(&bytes[0], bytes.size(), vertexShader)) {
        return false;
    }

    if (!ReadFileBytes("./shaders/shader.frag.spv", bytes) || !CompileShader(&bytes[0], bytes.size(), fragmentShader)) {
        return false;
    }

    VkPipelineShaderStageCreateInfo vertexStage = {};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = vertexShader;
    vertexStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentStage = {};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = fragmentShader;
    fragmentStage.pName = "main";

    const VkPipelineShaderStageCreateInfo shaderStages[] = {vertexStage, fragmentStage};

    const VkVertexInputBindingDescription bindings[] = {
        // binding; stride; inputRate;
        {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX},
    };

    const VkVertexInputAttributeDescription attributes[] = {
        // location; binding; format; offset;
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, position)},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, (uint32_t)offsetof(Vertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, (uint32_t)offsetof(Vertex, texCoord)},
        {3, 0, VK_FORMAT_R32G32B32A32_SINT, (uint32_t)offsetof(Vertex, joints)},
        {4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, (uint32_t)offsetof(Vertex, weights)},
    };

    VkPipelineVertexInputStateCreateInfo vertexInputCI = {};
    vertexInputCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCI.pNext = nullptr;
    vertexInputCI.vertexBindingDescriptionCount = ARRAY_COUNT(bindings);
    vertexInputCI.pVertexBindingDescriptions = bindings;
    vertexInputCI.vertexAttributeDescriptionCount = ARRAY_COUNT(attributes);
    vertexInputCI.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI = {};
    inputAssemblyCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportCI = {};
    viewportCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportCI.viewportCount = 1;
    viewportCI.pViewports = nullptr;
    viewportCI.scissorCount = 1;
    viewportCI.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizationCI = {};
    rasterizationCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationCI.depthClampEnable = VK_FALSE;
    rasterizationCI.rasterizerDiscardEnable = VK_FALSE;
    rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCI.cullMode = VK_CULL_MODE_NONE;
    rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationCI.depthBiasEnable = VK_FALSE;
    // rasterizationCI.depthBiasConstantFactor;
    // rasterizationCI.depthBiasClamp;
    // rasterizationCI.depthBiasSlopeFactor;
    rasterizationCI.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleCI = {};
    multisampleCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    // multisampleCI.sampleShadingEnable;
    // multisampleCI.minSampleShading;
    // multisampleCI.pSampleMask;
    // multisampleCI.alphaToCoverageEnable;
    // multisampleCI.alphaToOneEnable;

    VkPipelineDepthStencilStateCreateInfo depthStencilCI = {};
    depthStencilCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCI.depthTestEnable = VK_TRUE;
    depthStencilCI.depthWriteEnable = VK_TRUE;
    depthStencilCI.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilCI.stencilTestEnable = VK_FALSE;
    depthStencilCI.minDepthBounds = 0.0f;
    depthStencilCI.maxDepthBounds = 1.0f;

    VkPipelineColorBlendAttachmentState colorAttachmentInfo = {};
    colorAttachmentInfo.blendEnable = VK_FALSE;
    // colorAttachmentInfo.srcColorBlendFactor;
    // colorAttachmentInfo.dstColorBlendFactor;
    // colorAttachmentInfo.colorBlendOp;
    // colorAttachmentInfo.srcAlphaBlendFactor;
    // colorAttachmentInfo.dstAlphaBlendFactor;
    // colorAttachmentInfo.alphaBlendOp;
    colorAttachmentInfo.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendCI = {};
    colorBlendCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendCI.attachmentCount = 1;
    colorBlendCI.pAttachments = &colorAttachmentInfo;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};

    VkPipelineDynamicStateCreateInfo dynamicCI = {};
    dynamicCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicCI.dynamicStateCount = sizeof(dynamicStates) / sizeof(dynamicStates[0]);
    dynamicCI.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo renderingCI = {};
    renderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCI.colorAttachmentCount = 1;
    renderingCI.pColorAttachmentFormats = &m_swapchainFormat;
    renderingCI.depthAttachmentFormat = m_depthBufferFormat;
    // renderingCI.stencilAttachmentFormat;

    VkGraphicsPipelineCreateInfo pipelineCI = {};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.pNext = &renderingCI;
    pipelineCI.stageCount = ARRAY_COUNT(shaderStages);
    pipelineCI.pStages = shaderStages;
    pipelineCI.pVertexInputState = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyCI;
    pipelineCI.pTessellationState = nullptr;
    pipelineCI.pViewportState = &viewportCI;
    pipelineCI.pRasterizationState = &rasterizationCI;
    pipelineCI.pMultisampleState = &multisampleCI;
    pipelineCI.pDepthStencilState = &depthStencilCI;
    pipelineCI.pColorBlendState = &colorBlendCI;
    pipelineCI.pDynamicState = &dynamicCI;
    pipelineCI.layout = m_pipelineLayout;
    pipelineCI.renderPass = nullptr;
    VK_CHECK(vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipelineCI, nullptr, &m_pipeline));

    vkDestroyShaderModule(m_device, vertexShader, nullptr);
    vkDestroyShaderModule(m_device, fragmentShader, nullptr);

    return true;
}

bool Renderer::AllocateDeviceMemory(VkMemoryRequirements requirements, VkMemoryPropertyFlags propertyFlags,
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

void Renderer::DestroyDepthBuffer()
{
    vkDestroyImageView(m_device, m_depthBufferImageView, nullptr);
    vkFreeMemory(m_device, m_depthBufferMemory, nullptr);
    vkDestroyImage(m_device, m_depthBufferImage, nullptr);
}

bool Renderer::CreateDepthBuffer()
{
    m_depthBufferFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = m_depthBufferFormat;
    imageCI.extent.width = m_swapchainExtent.width;
    imageCI.extent.height = m_swapchainExtent.height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VK_CHECK(vkCreateImage(m_device, &imageCI, nullptr, &m_depthBufferImage));

    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(m_device, m_depthBufferImage, &requirements);
    VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    AllocateDeviceMemory(requirements, propertyFlags, m_depthBufferMemory);
    VK_CHECK(vkBindImageMemory(m_device, m_depthBufferImage, m_depthBufferMemory, 0));

    VkImageViewCreateInfo imageViewCI = {};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.image = m_depthBufferImage;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = m_depthBufferFormat;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(m_device, &imageViewCI, nullptr, &m_depthBufferImageView));

    return true;
}

bool Renderer::CreateBuffer(VkBufferUsageFlags usage, VkDeviceSize size, AllocatedBuffer &outBuffer)
{
    VkBufferCreateInfo bufferCI = {};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = size;
    bufferCI.usage = usage;
    VK_CHECK(vkCreateBuffer(m_device, &bufferCI, nullptr, &outBuffer.buffer));

    VkMemoryRequirements requirements = {};
    vkGetBufferMemoryRequirements(m_device, outBuffer.buffer, &requirements);
    VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    AllocateDeviceMemory(requirements, propertyFlags, outBuffer.memory);

    VK_CHECK(vkBindBufferMemory(m_device, outBuffer.buffer, outBuffer.memory, 0));
    outBuffer.size = size;
    VK_CHECK(vkMapMemory(m_device, outBuffer.memory, 0, outBuffer.size, 0, &outBuffer.data));

    return true;
}

bool Renderer::InitVulkan(GLFWwindow *window)
{
    return CreateInstance() && CreateSurface(window) && ChoosePhysicalDevice() && CreateDevice() &&
           CreateSwapchain(window) && CreateDepthBuffer() && CreateDescriptorSetLayouts() && CreateFrameData() &&
           CreatePipelineLayouts() && CreateGraphicsPipelines();
}

static void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
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

//
// Model Loader
//

static void LoadNode(const cgltf_data *gltf, const cgltf_node *node, Node &outNode)
{
    outNode.scale = node->has_scale ? glm::make_vec3(node->scale) : glm::vec3(1);
    outNode.translation = node->has_translation ? glm::make_vec3(node->translation) : glm::vec3(0);
    outNode.rotation = node->has_rotation ? glm::make_quat(node->rotation) : glm::quat();
    outNode.matrix = node->has_matrix ? glm::make_mat4(node->matrix) : glm::mat4(1);
    outNode.meshIndex = node->mesh ? cgltf_mesh_index(gltf, node->mesh) : UINT32_MAX;
    outNode.nodeIndex = cgltf_node_index(gltf, node);
    outNode.skinIndex = node->skin ? cgltf_skin_index(gltf, node->skin) : UINT32_MAX;

    for (const auto *child = node->children; child != node->children + node->children_count; ++child) {
        LoadNode(gltf, *child, outNode.children.emplace_back());
    }
}

static Node *GetNodeByIndex(Node &node, uint32_t nodeIndex)
{
    if (node.nodeIndex == nodeIndex) {
        return &node;
    }
    for (auto &child : node.children) {
        auto *res = GetNodeByIndex(child, nodeIndex);
        if (res != nullptr) {
            return res;
        }
    }

    return nullptr;
}

static inline InterpolationMethod ConvertInterpolation(cgltf_interpolation_type method)
{
    switch (method) {
    case cgltf_interpolation_type_linear:
        return InterpolationMethod_Linear;
    default:
        break;
    }
    assert(false && "Unhandled interpolation method");
    return InterpolationMethod_Linear;
}

static void LoadAnimations(const cgltf_data *gltf, Model &model)
{
    for (const auto *anim = gltf->animations; anim != gltf->animations + gltf->animations_count; ++anim) {
        Animation &outAnim = model.animations.emplace_back();
        for (const auto *chan = anim->channels; chan != anim->channels + anim->channels_count; ++chan) {
            const auto *sampler = chan->sampler;
            uint32_t nodeIndex = cgltf_node_index(gltf, chan->target_node);

            Node *node = GetNodeByIndex(model.rootNode, nodeIndex);
            if (!node) {
                continue;
            }

            AnimationSampler *outSampler = nullptr;
            for (uint32_t i = 0; i < outAnim.samplers.size(); ++i) {
                if (outAnim.samplers[i].node == node) {
                    outSampler = &outAnim.samplers[i];
                    break;
                }
            }

            if (outSampler == nullptr) {
                outAnim.samplers.emplace_back();
                outSampler = &outAnim.samplers.back();
                outSampler->node = node;
            }

            const float *times = nullptr;
            uint32_t timeCount = 0;

            { // Load times
                const auto *accessor = sampler->input;
                const auto *bufferView = accessor->buffer_view;
                const auto *buffer = bufferView->buffer;
                const auto *data = ((const uint8_t *)buffer->data) + bufferView->offset + accessor->offset;
                times = (const float *)data;
                timeCount = accessor->count;

                assert(accessor->component_type == cgltf_component_type_r_32f);
                assert(accessor->type == cgltf_type_scalar);

                for (uint32_t i = 0; i < accessor->count; ++i) {
                    if (times[i] > outAnim.endTime) {
                        outAnim.endTime = times[i];
                    }
                }
            }

            { // Load values
                const auto *accessor = sampler->output;
                const auto *bufferView = accessor->buffer_view;
                const auto *buffer = bufferView->buffer;
                const auto *data = ((const uint8_t *)buffer->data) + bufferView->offset + accessor->offset;
                const float *values = (const float *)data;

                assert(times != nullptr);

                switch (chan->target_path) {
                case cgltf_animation_path_type_translation:
                    for (uint32_t i = 0; i < accessor->count; ++i) {
                        outSampler->translation.values.push_back(glm::make_vec3(&values[i * 3]));
                        outSampler->translation.times.push_back(times[i]);
                    }
                    outSampler->translation.method = ConvertInterpolation(sampler->interpolation);
                    break;
                case cgltf_animation_path_type_scale:
                    for (uint32_t i = 0; i < accessor->count; ++i) {
                        outSampler->scale.values.push_back(glm::make_vec3(&values[i * 3]));
                        outSampler->scale.times.push_back(times[i]);
                    }
                    outSampler->scale.method = ConvertInterpolation(sampler->interpolation);
                    break;
                case cgltf_animation_path_type_rotation:
                    for (uint32_t i = 0; i < accessor->count; ++i) {
                        outSampler->rotation.values.push_back(glm::make_quat(&values[i * 4]));
                        outSampler->rotation.times.push_back(times[i]);
                    }
                    outSampler->rotation.method = ConvertInterpolation(sampler->interpolation);
                    break;
                }
            }
        }
    }
}

bool Renderer::LoadModel(const char *path)
{
    cgltf_options options = {};
    cgltf_data *gltf = nullptr;

    if (cgltf_parse_file(&options, path, &gltf) == cgltf_result_success) {
        if (cgltf_load_buffers(&options, gltf, path) == cgltf_result_success) {
            Model &model = m_models.emplace_back();
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            const auto *scene = gltf->scene;
            auto &rootNode = model.rootNode;
            for (const auto *node = scene->nodes; node != scene->nodes + scene->nodes_count; ++node) {
                LoadNode(gltf, *node, rootNode.children.emplace_back());
            }

            for (const auto *mesh = gltf->meshes; mesh != gltf->meshes + gltf->meshes_count; ++mesh) {
                const uint32_t primitiveOffset = (uint32_t)model.primitives.size();
                for (const auto *prim = mesh->primitives; prim != mesh->primitives + mesh->primitives_count; ++prim) {
                    uint32_t positionCount = 0;
                    const float *positions = nullptr;
                    const float *normals = nullptr;
                    const float *texCoords = nullptr;
                    const uint16_t *joints = nullptr;
                    const float *weights = nullptr;

                    for (const auto *attrib = prim->attributes; attrib != prim->attributes + prim->attributes_count;
                         ++attrib) {
                        const auto *accessor = attrib->data;
                        const auto *bufferView = accessor->buffer_view;
                        const auto *buffer = bufferView->buffer;
                        const auto *data = ((uint8_t *)buffer->data) + accessor->offset + bufferView->offset;

                        switch (attrib->type) {
                        case cgltf_attribute_type_position:
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            assert(accessor->type == cgltf_type_vec3);
                            positions = (const float *)data;
                            positionCount = accessor->count;
                            break;
                        case cgltf_attribute_type_normal:
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            assert(accessor->type == cgltf_type_vec3);
                            normals = (const float *)data;
                            break;
                        case cgltf_attribute_type_texcoord:
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            assert(accessor->type == cgltf_type_vec2);
                            texCoords = (const float *)data;
                            break;
                        case cgltf_attribute_type_joints:
                            assert(accessor->component_type == cgltf_component_type_r_16u);
                            assert(accessor->type == cgltf_type_vec4);
                            joints = (const uint16_t *)data;
                            break;
                        case cgltf_attribute_type_weights:
                            assert(accessor->component_type == cgltf_component_type_r_32f);
                            assert(accessor->type == cgltf_type_vec4);
                            weights = (const float *)data;
                            break;
                        }
                    }

                    const uint32_t indexOffset = (uint32_t)indices.size();
                    const uint32_t vertexOffset = (uint32_t)vertices.size();

                    if (prim->indices) {
                        const auto *accessor = prim->indices;
                        const auto *bufferView = accessor->buffer_view;
                        const auto *buffer = bufferView->buffer;
                        const auto *data = ((uint8_t *)buffer->data) + accessor->offset + bufferView->offset;

                        switch (accessor->component_type) {
                        case cgltf_component_type_r_32u:
                            for (uint32_t i = 0; i < accessor->count; ++i) {
                                indices.push_back(vertexOffset + ((const uint32_t *)data)[i]);
                            }
                            break;
                        case cgltf_component_type_r_16u:
                            for (uint32_t i = 0; i < accessor->count; ++i) {
                                indices.push_back(vertexOffset + ((const uint16_t *)data)[i]);
                            }
                            break;
                        }
                    } else {
                        for (uint32_t i = 0; i < positionCount; ++i) {
                            indices.push_back(vertexOffset + i);
                        }
                    }

                    for (uint32_t i = 0; i < positionCount; ++i) {
                        Vertex v = {};
                        v.position = glm::make_vec3(&positions[i * 3]);
                        v.normal = normals ? glm::make_vec3(&normals[i * 3]) : glm::vec3(0);
                        v.texCoord = texCoords ? glm::make_vec2(&texCoords[i * 2]) : glm::vec2(0);
                        v.joints = joints ? glm::ivec4(joints[i * 4 + 0], joints[i * 4 + 1], joints[i * 4 + 2],
                                                       joints[i * 4 + 3])
                                          : glm::ivec4(0);
                        v.weights = weights ? glm::make_vec4(&weights[i * 4]) : glm::vec4(0);
                        vertices.push_back(v);
                    }

                    auto &outPrim = model.primitives.emplace_back();
                    outPrim.indexOffset = indexOffset;
                    outPrim.indexCount = (uint32_t)indices.size() - indexOffset;
                }

                auto &outMesh = model.meshes.emplace_back();
                outMesh.primitiveOffset = primitiveOffset;
                outMesh.primitiveCount = (uint32_t)model.primitives.size() - primitiveOffset;
            }

            const VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
            const VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
            if (!CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBufferSize, model.vertexBuffer)) {
                return false;
            }
            memcpy(model.vertexBuffer.data, &vertices[0], vertexBufferSize);
            if (!CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBufferSize, model.indexBuffer)) {
                return false;
            }
            memcpy(model.indexBuffer.data, &indices[0], indexBufferSize);

            { // Load skins
                for (const auto *skin = gltf->skins; skin != gltf->skins + gltf->skins_count; ++skin) {
                    auto &outSkin = model.skins.emplace_back();

                    const auto *accessor = skin->inverse_bind_matrices;
                    const auto *bufferView = accessor->buffer_view;
                    const auto *buffer = bufferView->buffer;
                    const auto *data = ((const uint8_t *)buffer->data) + bufferView->offset + accessor->offset;
                    const float *values = (const float *)data;

                    uint32_t jointsCount = accessor->count;
                    uint32_t jointsBufferSize = jointsCount * sizeof(glm::mat4);

                    { // Per frame written data.

                        VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = {};
                        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                            layouts[i] = m_jointsDescriptorsLayout;
                        }

                        VkDescriptorSetAllocateInfo allocateInfo = {};
                        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                        allocateInfo.descriptorPool = m_descriptorPool;
                        allocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
                        allocateInfo.pSetLayouts = &layouts[0];

                        VK_CHECK(vkAllocateDescriptorSets(m_device, &allocateInfo, &outSkin.descriptorSet[0]));

                        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                            outSkin.jointMatrices[i].resize(jointsCount);

                            if (!CreateBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, jointsBufferSize,
                                              outSkin.jointMatricesBuffer[i])) {
                                return false;
                            }

                            VkDescriptorBufferInfo bufferInfo = {};
                            bufferInfo.buffer = outSkin.jointMatricesBuffer[i].buffer;
                            bufferInfo.offset = 0;
                            bufferInfo.range = outSkin.jointMatricesBuffer[i].size;

                            VkWriteDescriptorSet writeInfo = {};
                            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                            writeInfo.dstSet = outSkin.descriptorSet[i];
                            writeInfo.dstBinding = 0;
                            writeInfo.dstArrayElement = 0;
                            writeInfo.descriptorCount = 1;
                            writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                            writeInfo.pBufferInfo = &bufferInfo;

                            vkUpdateDescriptorSets(m_device, 1, &writeInfo, 0, nullptr);
                        }
                    }

                    for (uint32_t i = 0; i < accessor->count; ++i) {
                        uint32_t nodeIndex = cgltf_node_index(gltf, skin->joints[i]);
                        outSkin.inverseBindMatrices.push_back(glm::make_mat4(&values[i * 16]));
                        outSkin.joints.push_back(GetNodeByIndex(model.rootNode, nodeIndex));
                    }
                }
            }

            LoadAnimations(gltf, model);
        }

        cgltf_free(gltf);
    }
    return true;
}

//
// Rendering logic
//

void Model::UpdateAnimations(float dt)
{
    if (playingAnimation == nullptr) {
        return;
    }

    auto *animation = playingAnimation;
    animation_t += (float)dt;

    // TODO: Wrapping behaviour should be configured.
    if (animation_t > animation->endTime) {
        animation_t -= animation->endTime;
    }

    for (const auto &sampler : animation->samplers) {
        auto *node = sampler.node;
        assert(node != nullptr);
        sampler.scale.GetValueAtTime(animation_t, node->scale);
        sampler.translation.GetValueAtTime(animation_t, node->translation);
        sampler.rotation.GetValueAtTime(animation_t, node->rotation);
    }
}

void Model::UpdateTransforms()
{
    UpdateTransforms(glm::mat4(1), rootNode);
}

void Model::UpdateTransforms(glm::mat4 parentMatrix, Node &node)
{
    node.worldMatrix = parentMatrix * node.GetLocalMatrix();
    for (auto &child : node.children) {
        UpdateTransforms(node.worldMatrix, child);
    }
}

void Renderer::RenderNode(VkCommandBuffer commandBuffer, uint32_t frameIndex, Model &model, const Node &node)
{
    const auto &worldMatrix = node.worldMatrix;

    if (node.skinIndex != UINT32_MAX) {
        // NOTE:
        // Assuming each skin may only be referenced by 1 node.

        auto &skin = model.skins[node.skinIndex];
        const auto &inverseBindMatrices = skin.inverseBindMatrices;
        const auto &joints = skin.joints;
        auto &jointMatrices = skin.jointMatrices[frameIndex];
        auto &jointMatricesBuffer = skin.jointMatricesBuffer[frameIndex];
        auto descriptorSet = skin.descriptorSet[frameIndex];

        glm::mat4 rootNodeInverse = glm::inverse(worldMatrix);

        for (uint32_t i = 0; i < skin.joints.size(); ++i) {
            jointMatrices[i] = joints[i]->worldMatrix * inverseBindMatrices[i];
            jointMatrices[i] = rootNodeInverse * jointMatrices[i];
        }
        memcpy(jointMatricesBuffer.data, &jointMatrices[0], jointMatrices.size() * sizeof(jointMatrices[0]));

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &descriptorSet,
                                0, nullptr);
    }

    if (node.meshIndex != UINT32_MAX) {
        const auto &mesh = model.meshes[node.meshIndex];
        for (uint32_t i = 0; i < mesh.primitiveCount; ++i) {
            auto &prim = model.primitives[mesh.primitiveOffset + i];

            Constants constants = {};
            constants.model = worldMatrix;
            vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(constants),
                               &constants);
            vkCmdDrawIndexed(commandBuffer, prim.indexCount, 1, prim.indexOffset, 0, 0);
        }
    }

    for (const auto &child : node.children) {
        RenderNode(commandBuffer, frameIndex, model, child);
    }
}

bool Renderer::Init(GLFWwindow *window)
{
    VK_CHECK(volkInitialize());
    if (!InitVulkan(window)) {
        return false;
    }

#if 0
    // TODO: This is broken because pipeline expect some kind of skin to be bound. So either bind a default 
    // NULL-skin or create another pipeline.
    if (!LoadModel("./assets/DamagedHelmet.glb")) {
        return false;
    }
#else
#if 1
    if (!LoadModel("./assets/CesiumMan.glb")) {
        return false;
    }
#else
    if (!LoadModel("./assets/RiggedFigure.glb")) {
        return false;
    }
#endif

    Model &model = m_models.back();
    model.playingAnimation = &model.animations[0];
#endif

    const Vertex vertices[] = {
        {glm::vec3(-0.5f, -0.5f, 0.0f)},
        {glm::vec3(0.5f, -0.5f, 0.0f)},
        {glm::vec3(0.5f, 0.5f, 0.0f)},
    };
    if (!CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(vertices), m_vertexBuffer)) {
        return false;
    }
    memcpy(m_vertexBuffer.data, vertices, sizeof(vertices));
    return true;
}

void Renderer::DestroySwapchain()
{
    for (auto imageView : m_swapchainImageViews)
        vkDestroyImageView(m_device, imageView, nullptr);
    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
}

bool Renderer::HandleResize(GLFWwindow *window)
{
    VK_CHECK(vkDeviceWaitIdle(m_device));
    DestroyDepthBuffer();
    DestroySwapchain();
    if (!CreateSwapchain(window) || !CreateDepthBuffer())
        return false;
    return true;
}

bool Renderer::Render(const Camera &camera, GLFWwindow *window, double dt)
{
    if (!HandleResize(window)) {
        return false;
    }

    const uint32_t frameIndex = m_nextFrameIndex;
    m_nextFrameIndex = (m_nextFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    VK_CHECK(vkWaitForFences(m_device, 1, &m_commandBufferReady[frameIndex], VK_TRUE, ~0ull));
    VK_CHECK(vkResetFences(m_device, 1, &m_commandBufferReady[frameIndex]));

    uint32_t imageIndex = ~0u;
    VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, ~0ull, m_imageReady[frameIndex], nullptr, &imageIndex));

    for (auto &model : m_models) {
        model.UpdateAnimations((float)dt);
        model.UpdateTransforms();
    }

    VkCommandBuffer commandBuffer = m_commandBuffers[frameIndex];

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    {

        TransitionImageLayout(commandBuffer, m_swapchainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        TransitionImageLayout(commandBuffer, m_depthBufferImage, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRect2D renderArea = {};
        renderArea.extent = m_swapchainExtent;

        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_swapchainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color.float32[0] = 0.0f;
        colorAttachment.clearValue.color.float32[1] = 0.0f;
        colorAttachment.clearValue.color.float32[2] = 0.0f;
        colorAttachment.clearValue.color.float32[3] = 1.0f;

        VkRenderingAttachmentInfo depthAttachment = {};
        depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthAttachment.imageView = m_depthBufferImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.clearValue.depthStencil.depth = 1.0f;

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = renderArea;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRenderingKHR(commandBuffer, &renderingInfo);
        {
            float width = (float)m_swapchainExtent.width;
            float height = (float)m_swapchainExtent.height;

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            VkRect2D scissor = {};
            scissor.extent = m_swapchainExtent;
            VkViewport viewport = {};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = width;
            viewport.height = height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            float aspectRatio = width / height;
            glm::mat4 projection = glm::perspective(camera.fov, aspectRatio, camera.near, camera.far);
            if (camera.flipY)
                projection[1][1] *= -1;
            glm::mat4 view = glm::lookAt(camera.position, camera.target, camera.up);

            GlobalUniforms globalUniforms = {};
            globalUniforms.viewProjection = projection * view;
            ;
            memcpy(m_globalUniformBuffers[frameIndex].data, &globalUniforms, sizeof(globalUniforms));
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                    &m_globalDescriptors[frameIndex], 0, nullptr);

            for (auto &model : m_models) {
                VkDeviceSize vertexBufferOffset = 0;
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &model.vertexBuffer.buffer, &vertexBufferOffset);
                vkCmdBindIndexBuffer(commandBuffer, model.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                RenderNode(commandBuffer, frameIndex, model, model.rootNode);
            }
        }
        vkCmdEndRenderingKHR(commandBuffer);

        TransitionImageLayout(commandBuffer, m_swapchainImages[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    vkEndCommandBuffer(commandBuffer);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageReady[frameIndex];
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinished[frameIndex];
    VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_commandBufferReady[frameIndex]));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinished[frameIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;
    VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

    return true;
}

void Renderer::Shutdown()
{
}
