#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"
#include "Frost/Renderer/PipelineCompute.h"

namespace Frost
{

	class VulkanComputePipeline : public ComputePipeline
	{
	public:
		VulkanComputePipeline(ComputePipeline::CreateInfo& createInfo);
		virtual ~VulkanComputePipeline();

		virtual void Bind() override;
		virtual void Unbind() override;
		virtual void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;

		VkPipeline GetVulkanPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() const { return m_PipelineLayout; }
		void BindVulkanPushConstant(std::string name, void* data);

		virtual void Destroy() override;
	private:
		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
		VkPipelineCache m_PipelineCache;
		std::unordered_map<std::string, VkPushConstantRange> m_PushConstantRangeCache;
		ComputePipeline::CreateInfo m_Specification;
	};

}