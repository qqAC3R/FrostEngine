#include "frostpch.h"
#include "VulkanRayTracingPipeline.h"

#include "Frost/Renderer/Material.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/RayTracing/VulkanShaderBindingTable.h"

namespace Frost
{


	VulkanRayTracingPipeline::VulkanRayTracingPipeline(const RayTracingPipeline::CreateInfo& createInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		std::vector<VkDescriptorSetLayout> descriptorLayouts;
		auto hashMapped_descriptorLayouts = createInfo.Shader->GetVulkanDescriptorSetLayout();
		for (auto& descriptorLayout : hashMapped_descriptorLayouts)
			descriptorLayouts.push_back(descriptorLayout.second);


		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size());
		pipelineLayoutInfo.pushConstantRangeCount = createInfo.PushConstant.PushConstantRange > 0 ? 1 : 0;
													// If the range is higher than 0, then the count is 1
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
		if (createInfo.PushConstant.PushConstantRange > 0)
		{
			VkShaderStageFlags pushConstantStages{};
			for (auto stage : createInfo.PushConstant.PushConstantShaderStages)
			{
				pushConstantStages |= VulkanShader::GetShaderStageBits(stage);
			}

			VkPushConstantRange pushConstantRange{};
			pushConstantRange.offset = 0;
			pushConstantRange.size = createInfo.PushConstant.PushConstantRange;
			pushConstantRange.stageFlags = pushConstantStages;

			pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

			m_PushConstantRangeCache = pushConstantRange;
		}
		vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout);





		auto [shaderGroups, shaderStages] = createInfo.ShaderBindingTable->GetVulkanShaderInfo();

		VkRayTracingPipelineCreateInfoKHR raytracingPipelineInfo{};
		raytracingPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		raytracingPipelineInfo.stageCount = (uint32_t)shaderStages.size();
		raytracingPipelineInfo.pStages = shaderStages.data();
		raytracingPipelineInfo.groupCount = (uint32_t)shaderGroups.size();
		raytracingPipelineInfo.pGroups = shaderGroups.data();
		raytracingPipelineInfo.maxPipelineRayRecursionDepth = 2;
		raytracingPipelineInfo.layout = m_PipelineLayout;


		// TODO: Add multi-threaded pipeline creation
		VkDeferredOperationKHR defferedOperation{};

		vkCreateRayTracingPipelinesKHR(device, defferedOperation, {}, 1, &raytracingPipelineInfo, nullptr, &m_Pipeline);



		Ref<VulkanShaderBindingTable> vkShaderBindingTable = createInfo.ShaderBindingTable.As<VulkanShaderBindingTable>();
		vkShaderBindingTable->CreateShaderBindingTableBuffer(m_Pipeline);

	}

	VulkanRayTracingPipeline::~VulkanRayTracingPipeline()
	{
	}

	void VulkanRayTracingPipeline::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
	}

	void VulkanRayTracingPipeline::Bind(void* cmdBuf) const
	{
		if (!cmdBuf) return; // Currently cmdBuf is set to nullptr (by default if we dont specify it (for API reasons) )

		VkCommandBuffer vkCmdBuf = (VkCommandBuffer)cmdBuf;
		vkCmdBindPipeline(vkCmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline);
	}

	void VulkanRayTracingPipeline::Unbind() const
	{
	}

	void VulkanRayTracingPipeline::BindVulkanPushConstant(void* cmdBuf, void* data) const
	{
		auto vulkanCmdBuf = (VkCommandBuffer)cmdBuf;

		vkCmdPushConstants(vulkanCmdBuf, m_PipelineLayout,
						   m_PushConstantRangeCache.stageFlags,
						   m_PushConstantRangeCache.offset,
						   m_PushConstantRangeCache.size,
						   data);

	}


}