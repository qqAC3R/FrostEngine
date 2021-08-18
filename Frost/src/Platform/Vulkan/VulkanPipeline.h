#pragma once

#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/Pipeline.h"

namespace Frost
{

	// Push Constants(range, data)
	// DescriptorLayout
	// 
	// DepthStencilStateInfo depthStencilStateInfo {};
	// depthStencilStateInfo.DepthEnable = true;
	// depthStencilStateInfo.StencilEnable = false;
	// 
	// PipelineStates pipelineStates{};
	// pipelineStates.DepthStencil = &depthStencilStateInfo;
	// pipelineStates.Multisampling = nullptr;
	// 
	// BufferLayout
	// 
	// RenderPass
	// Shaders

	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(Pipeline::CreateInfo& createInfo);
		virtual ~VulkanPipeline();

		virtual void Bind(void* cmdBuf) override;
		virtual void Unbind() override {}

		/* Vulkan Specific */
		virtual VkPipeline GetVulkanPipeline() const override { return m_Pipeline; }
		virtual VkPipelineLayout GetVulkanPipelineLayout() const override { return m_PipelineLayout; }
		virtual void BindVulkanPushConstant(void* cmdBuf, void* data) override;

		virtual void Destroy() override;

	private:
		VkPipeline m_Pipeline;
		VkPipelineLayout m_PipelineLayout;

		VkPushConstantRange m_PushConstantRangeCache;
	};
}