#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"
#include "Frost/Renderer/PipelineCompute.h"

namespace Frost
{

	class VulkanComputePipeline : public ComputePipeline
	{
	public:
		VulkanComputePipeline(const ComputePipeline::CreateInfo& createInfo);
		virtual ~VulkanComputePipeline();

		virtual void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;
		void Dispatch(VkCommandBuffer cmdBuf, uint32_t groupX, uint32_t groupY, uint32_t groupZ);

		VkPipeline GetVulkanPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() const { return m_PipelineLayout; }
		void BindVulkanPushConstant(VkCommandBuffer cmdBuf, std::string name, void* data);

		virtual void Destroy() override;
	private:
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout;
		VkPipelineCache m_PipelineCache;
		std::unordered_map<std::string, VkPushConstantRange> m_PushConstantRangeCache;
	};

}