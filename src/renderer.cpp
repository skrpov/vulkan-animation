#include "renderer.h"
#include "cgltf.h"
#include "stb_image.h"

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

bool Renderer::CreateDescriptorSetLayouts()
{
    VkDevice device = m_device.GetDevice();

    const VkDescriptorPoolSize poolSizes[] = {
        // type; descriptorCount;
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4096},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096},
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI = {};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.maxSets = 4096;
    descriptorPoolCI.poolSizeCount = ARRAY_COUNT(poolSizes);
    descriptorPoolCI.pPoolSizes = poolSizes;
    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &m_descriptorPool));

    { // Global descriptors

        const VkDescriptorSetLayoutBinding bindings[] = {
            // binding; descriptorType; descriptorCount; stageFlags; pImmutableSamplers;
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo layoutCI = {};
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.bindingCount = ARRAY_COUNT(bindings);
        layoutCI.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &m_globalDescriptorsLayout));
    }

    { // Material descriptors
        const VkDescriptorSetLayoutBinding bindings[] = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };

        VkDescriptorSetLayoutCreateInfo layoutCI = {};
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.bindingCount = ARRAY_COUNT(bindings);
        layoutCI.pBindings = bindings;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &m_materialDescriptorsLayout));

        VkSamplerCreateInfo samplerCI = {};
        samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;  
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VK_CHECK(vkCreateSampler(device, &samplerCI, nullptr, &m_defaultSampler));
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
        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &m_jointsDescriptorsLayout));
    }

    return true;
}

bool Renderer::CreateFrameData()
{
    VkDevice device = m_device.GetDevice();
    VkCommandPool commandPool = m_device.GetCommandPool();

    {
        VkCommandBufferAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.commandPool = commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfo, m_commandBuffers));
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFenceCreateInfo fenceCI = {};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreCI = {};
        semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateFence(device, &fenceCI, nullptr, &m_commandBufferReady[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_imageReady[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCI, nullptr, &m_renderFinished[i]));
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
        VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &m_globalDescriptors[0]));

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            BufferCreateInfo bufferInfo = {};
            bufferInfo.size = sizeof(GlobalUniforms);
            bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.hostVisible = true;
            if (!m_device.CreateBuffer(bufferInfo, m_globalUniformBuffers[i])) {
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

        vkUpdateDescriptorSets(device, MAX_FRAMES_IN_FLIGHT, bufferWrites, 0, nullptr);
    }

    return true;
}

bool Renderer::CreatePipelineLayouts()
{
    VkDevice device = m_device.GetDevice();

    VkPushConstantRange range = {};
    range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    range.offset = 0;
    range.size = sizeof(Constants);

    const VkDescriptorSetLayout layouts[] = {m_globalDescriptorsLayout, m_jointsDescriptorsLayout, m_materialDescriptorsLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = ARRAY_COUNT(layouts);
    pipelineLayoutCI.pSetLayouts = layouts;
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &range;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &m_pipelineLayout));

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
        LOG_ERROR("Failed to read file at path %s", path);
        result = false;
    }

    return result;
}

bool Renderer::CompileShader(const void *bytes, uint32_t size, VkShaderModule &outShader)
{
    VkDevice device = m_device.GetDevice();

    VkShaderModuleCreateInfo shaderCI = {};
    shaderCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderCI.codeSize = size;
    shaderCI.pCode = (const uint32_t *)bytes;
    VK_CHECK(vkCreateShaderModule(device, &shaderCI, nullptr, &outShader));
    return true;
}

bool Renderer::CreateGraphicsPipelines()
{
    VkDevice device = m_device.GetDevice();
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

    VkFormat swapchainFormat = m_swapchain.GetFormat();

    VkPipelineRenderingCreateInfo renderingCI = {};
    renderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCI.colorAttachmentCount = 1;
    renderingCI.pColorAttachmentFormats = &swapchainFormat;
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
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &m_pipeline));

    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    return true;
}

void Renderer::DestroyDepthBuffer()
{
    VkDevice device = m_device.GetDevice();
    vkDestroyImageView(device, m_depthBufferImageView, nullptr);
    vkFreeMemory(device, m_depthBufferMemory, nullptr);
    vkDestroyImage(device, m_depthBufferImage, nullptr);
}

bool Renderer::CreateDepthBuffer()
{
    VkDevice device = m_device.GetDevice();
    VkExtent2D swapchainExtent = m_swapchain.GetExtent();
    m_depthBufferFormat = VK_FORMAT_D32_SFLOAT;

    VkImageCreateInfo imageCI = {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = m_depthBufferFormat;
    imageCI.extent.width = swapchainExtent.width;
    imageCI.extent.height = swapchainExtent.height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VK_CHECK(vkCreateImage(device, &imageCI, nullptr, &m_depthBufferImage));

    VkMemoryRequirements requirements = {};
    vkGetImageMemoryRequirements(device, m_depthBufferImage, &requirements);
    VkMemoryPropertyFlags propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    m_device.AllocateDeviceMemory(requirements, propertyFlags, m_depthBufferMemory);
    VK_CHECK(vkBindImageMemory(device, m_depthBufferImage, m_depthBufferMemory, 0));

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
    VK_CHECK(vkCreateImageView(device, &imageViewCI, nullptr, &m_depthBufferImageView));

    return true;
}

bool Renderer::InitVulkan(GLFWwindow *window)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    return CreateInstance() && CreateSurface(window) && m_device.Init(m_instance, m_surface) &&
           m_swapchain.Init(&m_device, m_surface, (uint32_t)width, (uint32_t)height) && CreateDepthBuffer() &&
           CreateDescriptorSetLayouts() && CreateFrameData() && CreatePipelineLayouts() && CreateGraphicsPipelines();
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

            { // Load times
                const auto *accessor = sampler->input;
                const auto *bufferView = accessor->buffer_view;
                const auto *buffer = bufferView->buffer;
                const auto *data = ((const uint8_t *)buffer->data) + bufferView->offset + accessor->offset;
                times = (const float *)data;

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

static bool LoadTextureView(VulkanDevice &device, const std::string &modelPath, cgltf_texture_view textureView, 
                            AllocatedImage &outImage)
{
    if (!textureView.texture) {
        return false;
    }

    auto *texture = textureView.texture;
    auto *image = texture->image;
    uint8_t *imageData = nullptr;
    int width = 0;
    int height = 0;
    int channels  = 0;

    if (image->buffer_view) {
        auto *bufferView = image->buffer_view;
        auto *buffer = bufferView->buffer;
        auto *data = ((uint8_t *)buffer->data) + bufferView->offset;

        imageData = stbi_load_from_memory(data, bufferView->size, &width, &height, &channels, 4);
    } else if (image->uri) {
        std::string fullPath = modelPath.substr(0, modelPath.find_last_of('/') + 1) + image->uri;

        imageData = stbi_load(fullPath.c_str(), &width, &height, &channels, 4);
    }

    if (!imageData) {
        return false;
    }

    ImageCreateInfo imageInfo = {};
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;

    if (!device.CreateImage(imageInfo, outImage)) {
        return false;
    }
    if (!device.SetImageData(outImage, imageData)) {
        return false;
    }

    stbi_image_free(imageData);

    return true;
}

bool Renderer::LoadModel(Scene &scene, const char *path)
{
    VkDevice device = m_device.GetDevice();
    cgltf_options options = {};
    cgltf_data *gltf = nullptr;

    if (cgltf_parse_file(&options, path, &gltf) == cgltf_result_success) {
        if (cgltf_load_buffers(&options, gltf, path) == cgltf_result_success) {
            Model &model = scene.models.emplace_back();
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            const auto *scene = gltf->scene;
            auto &rootNode = model.rootNode;
            for (const auto *node = scene->nodes; node != scene->nodes + scene->nodes_count; ++node) {
                LoadNode(gltf, *node, rootNode.children.emplace_back());
            }

            for (const auto *mat = gltf->materials; mat != gltf->materials + gltf->materials_count; ++mat) {
                Material &outMat = model.materials.emplace_back();
                std::string modelPath = path;

                if (mat->has_pbr_metallic_roughness) {
                    auto mr = mat->pbr_metallic_roughness;
                    if (!LoadTextureView(m_device, modelPath, mr.base_color_texture, outMat.albedoMap)) {
                        LOG_ERROR("Failed to load albedo texture for model");
                        return false;
                    }

                    {
                        MaterialUniforms uniforms = {};
                        uniforms.albedoFactor = glm::make_vec4(mr.base_color_factor);
                        uniforms.metallicFactor = mr.metallic_factor;
                        uniforms.roughnessFactor = mr.roughness_factor;

                        BufferCreateInfo bufferInfo = {};
                        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                        bufferInfo.size = sizeof(uniforms);
                        bufferInfo.hostVisible = true;

                        if (!m_device.CreateBuffer(bufferInfo, outMat.uniforms)) {
                            return false;
                        }
                        if (!m_device.SetBufferData(outMat.uniforms, 0, sizeof(uniforms), &uniforms)) {
                            return false;
                        }
                    }

                    VkDescriptorSetAllocateInfo allocateInfo = {};
                    allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    allocateInfo.descriptorPool = m_descriptorPool;
                    allocateInfo.descriptorSetCount = 1;
                    allocateInfo.pSetLayouts = &m_materialDescriptorsLayout;
                    VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &outMat.descriptorSet));

                    assert(m_defaultSampler != nullptr);

                    VkDescriptorImageInfo imageInfo = {};
                    imageInfo.sampler = m_defaultSampler;
                    imageInfo.imageView = outMat.albedoMap.imageView;
                    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

                    VkWriteDescriptorSet writeSet = {};
                    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeSet.dstSet = outMat.descriptorSet;
                    writeSet.dstBinding = 0;
                    writeSet.dstArrayElement = 0;
                    writeSet.descriptorCount = 1;
                    writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    writeSet.pImageInfo = &imageInfo;

                    VkDescriptorBufferInfo bufferInfo = {};
                    bufferInfo.buffer = outMat.uniforms.buffer;
                    bufferInfo.offset = 0;
                    bufferInfo.range = sizeof(MaterialUniforms);

                    VkWriteDescriptorSet writeSet2 = {};
                    writeSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writeSet2.dstSet = outMat.descriptorSet;
                    writeSet2.dstBinding = 1;
                    writeSet2.dstArrayElement = 0;
                    writeSet2.descriptorCount = 1;
                    writeSet2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    writeSet2.pBufferInfo = &bufferInfo;

                    const VkWriteDescriptorSet writeSets[] = {writeSet, writeSet2};

                    vkUpdateDescriptorSets(device, ARRAY_COUNT(writeSets), writeSets, 0, nullptr);

                } else {
                    LOG_ERROR("Model does not support pbr metallic roughness workflow");
                    return false;
                }
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

                    assert(texCoords);

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

                    assert(texCoords != nullptr);

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
                    outPrim.material = prim->material ? &model.materials[cgltf_material_index(gltf, prim->material)] : nullptr;
                }

                auto &outMesh = model.meshes.emplace_back();
                outMesh.primitiveOffset = primitiveOffset;
                outMesh.primitiveCount = (uint32_t)model.primitives.size() - primitiveOffset;
            }

            const VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
            const VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

            {
                BufferCreateInfo bufferInfo = {};
                bufferInfo.size = vertexBufferSize;
                bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                if (!m_device.CreateBuffer(bufferInfo, model.vertexBuffer) ||
                    !m_device.SetBufferData(model.vertexBuffer, 0, vertexBufferSize, vertices.data())) {
                    return false;
                }
            }
            {
                BufferCreateInfo bufferInfo = {};
                bufferInfo.size = vertexBufferSize;
                bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
                if (!m_device.CreateBuffer(bufferInfo, model.indexBuffer) ||
                    !m_device.SetBufferData(model.indexBuffer, 0, indexBufferSize, indices.data())) {
                    return false;
                }
            }

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

                        VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo, &outSkin.descriptorSet[0]));

                        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
                            outSkin.jointMatrices[i].resize(jointsCount);

                            {
                                BufferCreateInfo bufferInfo = {};
                                bufferInfo.size = jointsBufferSize;
                                bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                                bufferInfo.hostVisible = true;
                                if (!m_device.CreateBuffer(bufferInfo, outSkin.jointMatricesBuffer[i])) {
                                    return false;
                                }
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

                            vkUpdateDescriptorSets(device, 1, &writeInfo, 0, nullptr);
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
        m_device.SetBufferData(jointMatricesBuffer, 0, jointMatrices.size() * sizeof(jointMatrices[0]),
                               jointMatrices.data());

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1, &descriptorSet,
                                0, nullptr);
    }

    if (node.meshIndex != UINT32_MAX) {
        const auto &mesh = model.meshes[node.meshIndex];
        for (uint32_t i = 0; i < mesh.primitiveCount; ++i) {
            auto &prim = model.primitives[mesh.primitiveOffset + i];

            if (prim.material != nullptr) {
                vkCmdBindDescriptorSets(
                    commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                    m_pipelineLayout, 2, 1, &prim.material->descriptorSet, 0, nullptr);
            }

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

    m_isInitilized = true;
    return true;
}

bool Renderer::HandleResize(GLFWwindow *window)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkDevice device = m_device.GetDevice();
    VK_CHECK(vkDeviceWaitIdle(device));
    DestroyDepthBuffer();
    m_swapchain.Shutdown();
    if (!m_swapchain.Init(&m_device, m_surface, (uint32_t)width, (uint32_t)height) || !CreateDepthBuffer()) {
        return false;
    }
    return true;
}

bool Renderer::Render(Scene &scene, GLFWwindow *window, double dt)
{
    const auto &camera = scene.camera;
    auto &models = scene.models;

    if (!m_device.WaitForTransfers()) {
        return false;
    }

    if (!HandleResize(window)) {
        return false;
    }

    VkDevice device = m_device.GetDevice();
    VkSwapchainKHR swapchain = m_swapchain.GetSwapchain();

    const uint32_t frameIndex = m_nextFrameIndex;
    m_nextFrameIndex = (m_nextFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    VK_CHECK(vkWaitForFences(device, 1, &m_commandBufferReady[frameIndex], VK_TRUE, ~0ull));
    VK_CHECK(vkResetFences(device, 1, &m_commandBufferReady[frameIndex]));

    uint32_t imageIndex = ~0u;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, ~0ull, m_imageReady[frameIndex], nullptr, &imageIndex));

    VkImage swapchainImage = m_swapchain.GetImage(imageIndex);
    VkImageView swapchainImageView = m_swapchain.GetImageView(imageIndex);

    for (auto &model : models) {
        model.UpdateAnimations((float)dt);
        model.UpdateTransforms();
    }

    VkCommandBuffer commandBuffer = m_commandBuffers[frameIndex];

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    {

        TransitionImageLayout(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        TransitionImageLayout(commandBuffer, m_depthBufferImage, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRect2D renderArea = {};
        renderArea.extent = m_swapchain.GetExtent();

        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = swapchainImageView;
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
            VkExtent2D swapchainExtent = m_swapchain.GetExtent();
            float width = (float)swapchainExtent.width;
            float height = (float)swapchainExtent.height;

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

            VkRect2D scissor = {};
            scissor.extent = swapchainExtent;
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
            globalUniforms.cameraPosition = camera.position;
            m_device.SetBufferData(m_globalUniformBuffers[frameIndex], 0, sizeof(globalUniforms), &globalUniforms);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                    &m_globalDescriptors[frameIndex], 0, nullptr);

            for (auto &model : models) {
                VkDeviceSize vertexBufferOffset = 0;
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, &model.vertexBuffer.buffer, &vertexBufferOffset);
                vkCmdBindIndexBuffer(commandBuffer, model.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
                RenderNode(commandBuffer, frameIndex, model, model.rootNode);
            }
        }
        vkCmdEndRenderingKHR(commandBuffer);

        TransitionImageLayout(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
    VK_CHECK(vkQueueSubmit(m_device.GetGraphicsQueue(), 1, &submitInfo, m_commandBufferReady[frameIndex]));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinished[frameIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    VK_CHECK(vkQueuePresentKHR(m_device.GetPresentQueue(), &presentInfo));

    return true;
}

void Renderer::Shutdown()
{
    if (!m_isInitilized) {
        return;
    }
    m_isInitilized = false;

    vkDeviceWaitIdle(m_device.GetDevice());
    m_swapchain.Shutdown();
    m_device.Shutdown();
}
