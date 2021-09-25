#pragma once

#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/Framebuffer.h"

#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{

	class VulkanRenderPass : public RenderPass
	{
	public:
		VulkanRenderPass(const RenderPassSpecification& renderPassSpecs);
		virtual ~VulkanRenderPass();

		virtual Ref<Framebuffer> GetFramebuffer(uint32_t index) const override { return m_Framebuffers[index]; }
		virtual Ref<Image2D> GetColorAttachment(uint32_t attachmentSlot, uint32_t index) const override {
			return m_Framebuffers[index]->GetColorAttachment(attachmentSlot);
		}

		virtual const RenderPassSpecification& GetSpecification() const override { return m_Specification; }

		virtual void Destroy() override;
		
		virtual void Bind() override;
		virtual void Unbind() override;

		VkRenderPass GetVulkanRenderPass() const { return m_RenderPass; }
	private:
		Vector<Ref<Framebuffer>> m_Framebuffers;
		VkRenderPass m_RenderPass;

		RenderPassSpecification m_Specification;
		std::unordered_map<uint32_t, VkImageLayout> m_AttachmentLayouts;
	};

}