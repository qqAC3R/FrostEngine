#pragma once

#include "Frost/Renderer/Texture.h"

namespace Frost
{

	enum class FramebufferTextureFormat
	{
		None = 0,

		// Color
		RGBA8 = 1, RGBA16F = 2, RGBA32F = 3,
		R8 = 4,
		R32 = 7, R32I = 8,

		// Depth/stencil
		DEPTH32 = 5,
		DEPTH24STENCIL8 = 6,

		// Defaults
		Depth = DEPTH32,
		DepthStencil = DEPTH24STENCIL8
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format)
			: TextureFormat(format) {}

		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
	};

	struct FramebufferAttachmentSpecification
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: Attachments(attachments) {}

		Vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		FramebufferAttachmentSpecification Attachments;
		uint32_t Width, Height;
	};

	class RenderPass;

	class Framebuffer
	{
	public:
		virtual ~Framebuffer() {}


		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual const FramebufferSpecification& GetSpecification() const = 0;

		virtual void* GetFramebufferHandle() const = 0;
		virtual const Ref<Image2D>& GetColorAttachment(uint32_t index = 0) const = 0;
		virtual Vector<Ref<Image2D>> GetColorAttachments() const = 0;

		/* OpenGL Specific API */
		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void Unbind(uint32_t slot = 0) const = 0;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;

		virtual void Destroy() = 0;

		/* Vulkan Specific constructor */
		//static Ref<Framebuffer> Create(Ref<RenderPass> renderPass, const FramebufferSpecification& spec = {});
		static Ref<Framebuffer> Create(const FramebufferSpecification& spec = {});
	};

}