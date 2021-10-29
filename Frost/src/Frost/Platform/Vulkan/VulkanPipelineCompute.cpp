#include "frostpch.h"
#include "VulkanPipelineCompute.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"

namespace Frost
{
	VulkanComputePipeline::VulkanComputePipeline(ComputePipeline::CreateInfo& createInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto shaders = createInfo.Shader.As<VulkanShader>();
		auto descriptorLayouts = Utils::GetDescriptorSetLayoutFromHashMap(createInfo.Shader->GetVulkanDescriptorSetLayout());
		auto pushConstants = Utils::GetPushConstantRangesFromVector(createInfo.Shader->GetShaderReflectionData().GetPushConstantData());
		m_PushConstantRangeCache = Utils::GetPushConstantCacheFromVector(createInfo.Shader->GetShaderReflectionData().GetPushConstantData());

		Vector<VkPipelineShaderStageCreateInfo> shaderStages;
		for (const auto& shader : shaders->m_ShaderModules)
		{
			VkPipelineShaderStageCreateInfo shaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
			shaderStageInfo.stage = shader.first;;
			shaderStageInfo.module = shader.second;
			shaderStageInfo.pName = "main";
			shaderStageInfo.pSpecializationInfo = nullptr;
			shaderStages.push_back(shaderStageInfo);
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorLayouts.size();
		pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
		pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();
		FROST_VKCHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout));


		VkComputePipelineCreateInfo computePipelineCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		computePipelineCreateInfo.layout = m_PipelineLayout;
		computePipelineCreateInfo.flags = 0;
		computePipelineCreateInfo.stage = shaderStages[0];

		VkPipelineCacheCreateInfo pipelineCacheCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
		FROST_VKCHECK(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &m_PipelineCache));
		FROST_VKCHECK(vkCreateComputePipelines(device, m_PipelineCache, 1, &computePipelineCreateInfo, nullptr, &m_Pipeline));
	}

	VulkanComputePipeline::~VulkanComputePipeline()
	{
		Destroy();
	}

	void VulkanComputePipeline::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
	{
		FROST_ASSERT_MSG("VulkanComputePipeline::Dispatch needs a command buffer!");
	}

	void VulkanComputePipeline::Dispatch(VkCommandBuffer cmdBuf, uint32_t groupX, uint32_t groupY, uint32_t groupZ)
	{
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
		vkCmdDispatch(cmdBuf, groupX, groupY, groupZ);
	}

	void VulkanComputePipeline::BindVulkanPushConstant(VkCommandBuffer cmdBuf, std::string name, void* data)
	{
		vkCmdPushConstants(cmdBuf, m_PipelineLayout,
						   m_PushConstantRangeCache[name].stageFlags,
						   m_PushConstantRangeCache[name].offset,
						   m_PushConstantRangeCache[name].size,
						   data);
	}

	void VulkanComputePipeline::Destroy()
	{
		if (m_Pipeline == VK_NULL_HANDLE) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineCache(device, m_PipelineCache, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);

		m_Pipeline = VK_NULL_HANDLE;
	}
}