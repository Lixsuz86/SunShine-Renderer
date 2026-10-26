#include "vulkan_rhi_pipeline.hpp"
#include "vulkan_rhi_shader.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "sunshine/error.hpp"
#include <array>

namespace Sunshine::Vulkan {

VkFormat VulkanRHIPipeline::ConvertVertexFormat(RHI::VertexFormat format) {
    switch (format) {
        case RHI::VertexFormat::Float:  return VK_FORMAT_R32_SFLOAT;
        case RHI::VertexFormat::Float2: return VK_FORMAT_R32G32_SFLOAT;
        case RHI::VertexFormat::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
        case RHI::VertexFormat::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        default: return VK_FORMAT_UNDEFINED;
    }
}

VkPrimitiveTopology VulkanRHIPipeline::ConvertTopology(RHI::PrimitiveTopology topology) {
    switch (topology) {
        case RHI::PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case RHI::PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case RHI::PrimitiveTopology::LineList:       return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case RHI::PrimitiveTopology::PointList:      return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        default: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
}

VkCullModeFlags VulkanRHIPipeline::ConvertCullMode(RHI::CullMode mode) {
    switch (mode) {
        case RHI::CullMode::None:  return VK_CULL_MODE_NONE;
        case RHI::CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
        case RHI::CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        default: return VK_CULL_MODE_NONE;
    }
}

VkFrontFace VulkanRHIPipeline::ConvertFrontFace(RHI::FrontFace face) {
    switch (face) {
        case RHI::FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
        case RHI::FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
        default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
}

VkPolygonMode VulkanRHIPipeline::ConvertPolygonMode(RHI::PolygonMode mode) {
    switch (mode) {
        case RHI::PolygonMode::Fill:  return VK_POLYGON_MODE_FILL;
        case RHI::PolygonMode::Line:  return VK_POLYGON_MODE_LINE;
        case RHI::PolygonMode::Point: return VK_POLYGON_MODE_POINT;
        default: return VK_POLYGON_MODE_FILL;
    }
}

VkCompareOp VulkanRHIPipeline::ConvertCompareOp(RHI::CompareOp op) {
    switch (op) {
        case RHI::CompareOp::Never:          return VK_COMPARE_OP_NEVER;
        case RHI::CompareOp::Less:           return VK_COMPARE_OP_LESS;
        case RHI::CompareOp::Equal:          return VK_COMPARE_OP_EQUAL;
        case RHI::CompareOp::LessOrEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
        case RHI::CompareOp::Greater:        return VK_COMPARE_OP_GREATER;
        case RHI::CompareOp::NotEqual:       return VK_COMPARE_OP_NOT_EQUAL;
        case RHI::CompareOp::GreaterOrEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case RHI::CompareOp::Always:         return VK_COMPARE_OP_ALWAYS;
        default:                             return VK_COMPARE_OP_LESS;
    }
}

VulkanRHIPipeline::VulkanRHIPipeline(VulkanContext& context, VkRenderPass compatibleRenderPass, const RHI::PipelineDesc& desc)
    : m_context(context), m_desc(desc) 
{
    VkDevice device = m_context.GetDevice();

    // 1. Shader Stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    if (desc.vertexShader) {
        auto* vkVS = dynamic_cast<VulkanRHIShader*>(desc.vertexShader.get());
        if (!vkVS) throw GraphicsException("Invalid vertex shader passed to VulkanRHIPipeline!");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stageInfo.module = vkVS->GetModule();
        stageInfo.pName = vkVS->GetEntryPoint().c_str();
        shaderStages.push_back(stageInfo);
    } else {
        throw GraphicsException("PipelineDesc requires a valid vertexShader!");
    }

    if (desc.fragmentShader) {
        auto* vkFS = dynamic_cast<VulkanRHIShader*>(desc.fragmentShader.get());
        if (!vkFS) throw GraphicsException("Invalid fragment shader passed to VulkanRHIPipeline!");

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stageInfo.module = vkFS->GetModule();
        stageInfo.pName = vkFS->GetEntryPoint().c_str();
        shaderStages.push_back(stageInfo);
    }

    // 2. Vertex Input State
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    if (!desc.vertexLayout.bindings.empty()) {
        bindingDescriptions.reserve(desc.vertexLayout.bindings.size());
        for (const auto& b : desc.vertexLayout.bindings) {
            VkVertexInputBindingDescription bd{};
            bd.binding = b.binding;
            bd.stride = b.stride;
            bd.inputRate = (b.inputRate == RHI::VertexInputRate::PerInstance) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            bindingDescriptions.push_back(bd);
        }
    } else if (desc.vertexLayout.stride > 0) {
        VkVertexInputBindingDescription bd{};
        bd.binding = 0;
        bd.stride = desc.vertexLayout.stride;
        bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescriptions.push_back(bd);
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    attributeDescriptions.reserve(desc.vertexLayout.attributes.size());

    for (const auto& attr : desc.vertexLayout.attributes) {
        VkVertexInputAttributeDescription attrDesc{};
        attrDesc.binding = attr.binding;
        attrDesc.location = attr.location;
        attrDesc.format = ConvertVertexFormat(attr.format);
        attrDesc.offset = attr.offset;
        attributeDescriptions.push_back(attrDesc);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.empty() ? nullptr : bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.empty() ? nullptr : attributeDescriptions.data();

    // 3. Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = ConvertTopology(desc.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 4. Viewport State (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // 5. Dynamic States
    std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    // 6. Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = ConvertPolygonMode(desc.rasterizer.polygonMode);
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = ConvertCullMode(desc.rasterizer.cullMode);
    rasterizer.frontFace = ConvertFrontFace(desc.rasterizer.frontFace);
    rasterizer.depthBiasEnable = desc.rasterizer.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = desc.rasterizer.depthBiasConstantFactor;
    rasterizer.depthBiasClamp = desc.rasterizer.depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = desc.rasterizer.depthBiasSlopeFactor;

    // 7. Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 8. Depth & Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = ConvertCompareOp(desc.depthStencil.depthCompareOp);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 9. Color Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = desc.blend.blendEnable ? VK_TRUE : VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    if (desc.colorAttachmentCount == 0) {
        colorBlending.attachmentCount = 0;
        colorBlending.pAttachments = nullptr;
    } else {
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
    }

    // 10. Descriptor Set Layout & Pool
    constexpr uint32_t MAX_SETS = 2; // Support double-buffering
    if (!desc.descriptorBindings.empty()) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.descriptorBindings.size());

        for (const auto& b : desc.descriptorBindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = b.binding;
            binding.descriptorType = (b.type == RHI::DescriptorType::UniformBuffer)
                ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = 0;
            if (b.stageFlags == RHI::ShaderStage::Vertex) {
                binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            } else if (b.stageFlags == RHI::ShaderStage::Fragment) {
                binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            } else {
                binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
            }
            binding.pImmutableSamplers = nullptr;
            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout));

        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = MAX_SETS * 10;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = MAX_SETS * 10;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_SETS;

        VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool));

        std::vector<VkDescriptorSetLayout> layouts(MAX_SETS, m_descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = MAX_SETS;
        allocInfo.pSetLayouts = layouts.data();

        m_descriptorSets.resize(MAX_SETS);
        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, m_descriptorSets.data()));
    }

    // 11. Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    } else {
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
    }
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

    // 12. Create Graphics Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = compatibleRenderPass;
    pipelineInfo.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline));
}

VulkanRHIPipeline::~VulkanRHIPipeline() {
    VkDevice device = m_context.GetDevice();
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
}

VkDescriptorSet VulkanRHIPipeline::GetDescriptorSet(uint32_t frameIndex) const {
    if (m_descriptorSets.empty()) return VK_NULL_HANDLE;
    return m_descriptorSets[frameIndex % m_descriptorSets.size()];
}

void VulkanRHIPipeline::UpdateUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t frameIndex) {
    if (m_descriptorSets.empty()) return;
    VkDescriptorSet ds = GetDescriptorSet(frameIndex);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = offset;
    bufferInfo.range = range;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = ds;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_context.GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void VulkanRHIPipeline::UpdateTexture(uint32_t binding, VkImageView imageView, VkSampler sampler, uint32_t frameIndex) {
    if (m_descriptorSets.empty()) return;
    VkDescriptorSet ds = GetDescriptorSet(frameIndex);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = ds;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_context.GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

} // namespace Sunshine::Vulkan
