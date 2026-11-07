#include "vulkan_rhi_compute_pipeline.hpp"
#include "vulkan_rhi_shader.hpp"
#include "backend/vulkan/vulkan_context.hpp"
#include "sunshine/error.hpp"
#include <array>

namespace Sunshine::Vulkan {

VulkanRHIComputePipeline::VulkanRHIComputePipeline(VulkanContext& context, const RHI::ComputePipelineDesc& desc)
    : m_context(context), m_desc(desc)
{
    VkDevice device = m_context.GetDevice();

    if (!desc.computeShader) {
        throw GraphicsException("ComputePipelineDesc requires a valid computeShader!");
    }

    auto* vkCS = dynamic_cast<VulkanRHIShader*>(desc.computeShader.get());
    if (!vkCS) {
        throw GraphicsException("Invalid compute shader passed to VulkanRHIComputePipeline!");
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = vkCS->GetModule();
    stageInfo.pName = vkCS->GetEntryPoint().c_str();

    // Descriptors
    if (!desc.descriptorBindings.empty()) {
        constexpr uint32_t MAX_SETS = 2; // Double-buffering matching MAX_FRAMES_IN_FLIGHT

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.descriptorBindings.size());

        for (const auto& b : desc.descriptorBindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = b.binding;
            if (b.type == RHI::DescriptorType::UniformBuffer) {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            } else if (b.type == RHI::DescriptorType::StorageBuffer) {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            } else {
                binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
            binding.descriptorCount = 1;
            binding.stageFlags = 0;
            if (b.stageFlags & RHI::ShaderStage::Compute)  binding.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
            if (b.stageFlags & RHI::ShaderStage::Vertex)   binding.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
            if (b.stageFlags & RHI::ShaderStage::Fragment) binding.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            if (binding.stageFlags == 0)                   binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            binding.pImmutableSamplers = nullptr;
            bindings.push_back(binding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout));

        std::array<VkDescriptorPoolSize, 3> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = MAX_SETS * 10;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = MAX_SETS * 10;
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[2].descriptorCount = MAX_SETS * 10;

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

    // Pipeline Layout
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

    // Create Compute Pipeline
    VkComputePipelineCreateInfo computeInfo{};
    computeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computeInfo.stage = stageInfo;
    computeInfo.layout = m_pipelineLayout;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computeInfo, nullptr, &m_pipeline));
}

VulkanRHIComputePipeline::~VulkanRHIComputePipeline() {
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

VkDescriptorSet VulkanRHIComputePipeline::GetDescriptorSet(uint32_t frameIndex) const {
    if (m_descriptorSets.empty()) return VK_NULL_HANDLE;
    return m_descriptorSets[frameIndex % m_descriptorSets.size()];
}

void VulkanRHIComputePipeline::UpdateUniformBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t frameIndex) {
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

void VulkanRHIComputePipeline::UpdateStorageBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t frameIndex) {
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
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_context.GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void VulkanRHIComputePipeline::UpdateTexture(uint32_t binding, VkImageView imageView, VkSampler sampler, uint32_t frameIndex) {
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

