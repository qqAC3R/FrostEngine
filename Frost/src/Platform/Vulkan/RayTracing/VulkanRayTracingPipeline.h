#pragma once

#include "Frost/Renderer/RayTracing/RayTracingPipeline.h"

#include <vulkan/vulkan.hpp>

namespace Frost
{


	class VulkanRayTracingPipeline : public RayTracingPipeline
	{
	public:
		VulkanRayTracingPipeline(const RayTracingPipeline::CreateInfo& createInfo);
		virtual ~VulkanRayTracingPipeline();

		virtual void Destroy() override;

		virtual void Bind(void* cmdBuf) const override;
		virtual void Unbind() const override;

		/* Vulkan Specific */
		virtual VkPipeline GetVulkanPipeline() const override { return m_Pipeline; }
		virtual VkPipelineLayout GetVulkanPipelineLayout() const override { return m_PipelineLayout; }
		virtual void BindVulkanPushConstant(void* cmdBuf, void* data) const override;

	private:
		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;

		VkPushConstantRange m_PushConstantRangeCache;
	};

}