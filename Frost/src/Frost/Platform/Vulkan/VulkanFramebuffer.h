#pragma once

#include "Vulkan.h"
#include "Frost/Renderer/Framebuffer.h"

namespace Frost
{

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(Ref<RenderPass> renderPass, const FramebufferSpecification& spec);
		virtual ~VulkanFramebuffer();

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }

		virtual void* GetFramebufferHandle() const override { return m_Framebuffer; }
		virtual const Ref<Image2D>& GetColorAttachmentRendererID(uint32_t index = 0) const override {
			if (index > (uint32_t)m_Attachments.size()) { FROST_ASSERT(false, ""); }
			return m_Attachments[index];
		}
		virtual Vector<Ref<Image2D>> GetColorAttachments() const override { return m_Attachments; }

		virtual void Destroy() override;
		
	private:
		void CreateAttachments();
		void CreateFramebuffer(Ref<RenderPass> renderPass);

	private:

		VkFramebuffer m_Framebuffer;
		Vector<Ref<Image2D>> m_Attachments;

		FramebufferSpecification m_Specification;

		Vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
		FramebufferTextureSpecification m_DepthAttachmentSpecification = FramebufferTextureFormat::None;


	public:
		/* Function that are useless */
		virtual void Bind(uint32_t slot = 0) const override {}
		virtual void Unbind(uint32_t slot = 0) const override {}

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override {};
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override { return int(); }
	};

}