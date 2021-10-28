#pragma once

#include "Frost/Renderer/RayTracing/RayTracingPipeline.h"
#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{
	class VulkanRayTracingPipeline : public RayTracingPipeline
	{
	public:
		VulkanRayTracingPipeline(const RayTracingPipeline::CreateInfo& createInfo);
		virtual ~VulkanRayTracingPipeline();

		virtual void Destroy() override;

		virtual void Bind() const override;
		virtual void Unbind() const override;

		/* Vulkan Specific */
		VkPipeline GetVulkanPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetVulkanPipelineLayout() const { return m_PipelineLayout; }
		void BindVulkanPushConstant(std::string name, void* data);

	private:
		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;
		std::unordered_map<std::string, VkPushConstantRange> m_PushConstantRangeCache;
		RayTracingPipeline::CreateInfo m_Specification;
	};
}