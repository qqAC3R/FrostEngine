#pragma once

#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/Framebuffer.h"

#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{

	class VulkanRenderPass : public RenderPass
	{
	public:
		VulkanRenderPass(const FramebufferSpecification& framebufferSpecs);
		virtual ~VulkanRenderPass();

		VkRenderPass GetVulkanRenderPass() const { return m_RenderPass; }
		virtual void Destroy() override;
		
		virtual void Bind() override;
		virtual void Unbind() override;

		virtual Ref<Framebuffer> GetFramebuffer() override { return nullptr; }
	private:
		VkRenderPass m_RenderPass;
	public:
		// Useless functions
	};

}