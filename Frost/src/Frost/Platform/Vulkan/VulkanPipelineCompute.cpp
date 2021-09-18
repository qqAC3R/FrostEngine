#include "frostpch.h"
#include "VulkanPipelineCompute.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"

namespace Frost
{
	static VkFence s_ComputeFence = nullptr;

	VulkanComputePipeline::VulkanComputePipeline(ComputePipeline::CreateInfo& createInfo)
		: m_Specification(createInfo)
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

	}

	void VulkanComputePipeline::Bind()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetComputeCommandBuffer(currentFrameIndex);

		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		FROST_VKCHECK(vkBeginCommandBuffer(cmdBuf, &cmdBufferBeginInfo));

		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
	}

	void VulkanComputePipeline::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetComputeCommandBuffer(currentFrameIndex);

		vkCmdDispatch(cmdBuf, groupX, groupY, groupZ);

		vkEndCommandBuffer(cmdBuf);

		if (!s_ComputeFence)
		{
			VkFenceCreateInfo fenceCreateInfo{};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
			FROST_VKCHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &s_ComputeFence));
		}
		vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &s_ComputeFence);

		VkQueue computeQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().ComputeFamily.Queue;
		VkSubmitInfo computeSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &cmdBuf;
		FROST_VKCHECK(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, s_ComputeFence));

		vkWaitForFences(device, 1, &s_ComputeFence, VK_TRUE, UINT64_MAX);

	}

	void VulkanComputePipeline::Unbind()
	{
	}

	void VulkanComputePipeline::BindVulkanPushConstant(std::string name, void* data)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetComputeCommandBuffer(currentFrameIndex);

		vkCmdPushConstants(cmdBuf, m_PipelineLayout,
						m_PushConstantRangeCache[name].stageFlags,
						m_PushConstantRangeCache[name].offset,
						m_PushConstantRangeCache[name].size,
				data);
	}

	void VulkanComputePipeline::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyPipelineCache(device, m_PipelineCache, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyFence(device, s_ComputeFence, nullptr);
	}
}