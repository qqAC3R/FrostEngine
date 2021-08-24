#pragma once

#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/Framebuffer.h"

#include <vulkan/vulkan.hpp>

namespace Frost
{

	class VulkanRenderPass : public RenderPass
	{
	public:
		VulkanRenderPass(const FramebufferSpecification& framebufferSpecs);
		virtual ~VulkanRenderPass();

		virtual VkRenderPass GetVulkanRenderPass() override { return m_RenderPass; }
		virtual void Destroy() override;

		virtual Ref<Framebuffer> GetFramebuffer() override { return nullptr; }

	private:
		VkRenderPass m_RenderPass;

	public:
		// Useless functions
		virtual void Bind() override {}
		virtual void Unbind() override {}


	};

}