#include "frostpch.h"
#include "VulkanRayTracingPipeline.h"

#include "Frost/Renderer/Material.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanShaderBindingTable.h"

namespace Frost
{

	VulkanRayTracingPipeline::VulkanRayTracingPipeline(const RayTracingPipeline::CreateInfo& createInfo)
		: m_Specification(createInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		auto descriptorLayouts = Utils::GetDescriptorSetLayoutFromHashMap(createInfo.Shader->GetVulkanDescriptorSetLayout());
		auto pushConstants = Utils::GetPushConstantRangesFromVector(createInfo.Shader->GetShaderReflectionData().GetPushConstantData());
		m_PushConstantRangeCache = Utils::GetPushConstantCacheFromVector(createInfo.Shader->GetShaderReflectionData().GetPushConstantData());

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size());
		pipelineLayoutInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
		pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();
		vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout);


		Ref<VulkanShaderBindingTable> rtSbt = createInfo.ShaderBindingTable.As<VulkanShaderBindingTable>();
		auto [shaderGroups, shaderStages] = rtSbt->GetVulkanShaderInfo();

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

	void VulkanRayTracingPipeline::Bind() const
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_Pipeline);
	}

	void VulkanRayTracingPipeline::Unbind() const
	{
	}

	void VulkanRayTracingPipeline::BindVulkanPushConstant(std::string name, void* data)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		vkCmdPushConstants(cmdBuf, m_PipelineLayout,
						   m_PushConstantRangeCache[name].stageFlags,
						   m_PushConstantRangeCache[name].offset,
						   m_PushConstantRangeCache[name].size,
						   data);

	}


}