#pragma once

#include "Frost/Renderer/Framebuffer.h"

namespace Frost
{
	enum class OperationStore
	{
		Store, DontCare
	};

	enum class OperationLoad
	{
		Load, Clear, DontCare
	};

	struct RenderPassSpecification
	{
		struct RenderPassFramebufferSpec
		{
			FramebufferTextureSpecification FramebufferAttachmentSpec;
			TextureSpecs::UsageSpec FinalLayout;

			OperationLoad LoadOperation;
			OperationStore StoreOperation;
			OperationLoad DepthLoadOperation;
			OperationStore DepthStoreOperation;
		};

		RenderPassSpecification() = default;
		RenderPassSpecification(uint32_t width, uint32_t height, uint32_t framebufferCount, const std::initializer_list<RenderPassFramebufferSpec>& renderPassSpec)
			: RenderPassSpec(renderPassSpec), FramebufferCount(framebufferCount)
		{
			FramebufferSpecification.Width = width;
			FramebufferSpecification.Height = height;
			for (auto& attachment : renderPassSpec)
			{
				FramebufferSpecification.Attachments.Attachments.push_back(attachment.FramebufferAttachmentSpec);
			}
		}

		Vector<RenderPassFramebufferSpec> RenderPassSpec;
		FramebufferSpecification FramebufferSpecification;
		uint32_t FramebufferCount = 1;
	};

	class RenderPass
	{
	public:
		virtual ~RenderPass() {}

		virtual Ref<Framebuffer> GetFramebuffer(uint32_t index = 0) const = 0;
		virtual Ref<Image2D> GetColorAttachment(uint32_t attachmentSlot, uint32_t index = 0) const = 0;
		virtual const RenderPassSpecification& GetSpecification() const = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		static Ref<RenderPass> Create(const RenderPassSpecification& renderPassSpecs);
	};

}