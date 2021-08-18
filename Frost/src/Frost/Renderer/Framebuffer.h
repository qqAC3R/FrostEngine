#pragma once

#include "Frost/Renderer/Texture.h"

namespace Frost
{

	enum class FramebufferTextureFormat
	{
		None = 0,

		// Color
		RGBA8 = 1, RGBA16F = 2,

		// Depth/stencil
		DEPTH32 = 3,

		// Defaults
		Depth = DEPTH32,

		SWAPCHAIN = 4 // Lmao this is so bad (basically gets the texture format from the swapchain)
	};

	struct FramebufferComponents
	{
		void* Image;
	};

	struct FramebufferTextureSpecification
	{
		FramebufferTextureSpecification() = default;
		FramebufferTextureSpecification(FramebufferTextureFormat format, FramebufferComponents components = {})
			: TextureFormat(format), Components(components) {}

		FramebufferTextureFormat TextureFormat = FramebufferTextureFormat::None;
		FramebufferComponents Components = {};
	};

	struct FramebufferAttachmentSpecification
	{
		FramebufferAttachmentSpecification() = default;
		FramebufferAttachmentSpecification(std::initializer_list<FramebufferTextureSpecification> attachments)
			: Attachments(attachments) {}

		std::vector<FramebufferTextureSpecification> Attachments;
	};

	struct FramebufferSpecification
	{
		FramebufferAttachmentSpecification Attachments;
		uint32_t Width, Height;
	};

	

	class Framebuffer
	{
	public:
		//VulkanFramebuffer(const VkRenderPass& renderPass, const FramebufferSpecification& spec);
		virtual ~Framebuffer() {}


		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual const FramebufferSpecification& GetSpecification() const = 0;


		virtual void* GetRendererID() const = 0;
		virtual const Ref<Image2D>& GetColorAttachmentRendererID(uint32_t index = 0) const = 0;

		/* OpenGL Specific API */
		virtual void Bind(uint32_t slot = 0) const = 0;
		virtual void Unbind(uint32_t slot = 0) const = 0;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) = 0;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) = 0;


		/* Vulkan Specific API */
		virtual void Destroy() = 0;



		/* Vulkan Specific constructor */
		static Ref<Framebuffer> Create(void* renderPass, const FramebufferSpecification& spec = {});

		/* Opengl Specific constructor */
		static Ref<Framebuffer> Create(const FramebufferSpecification& spec = {});


	};

}