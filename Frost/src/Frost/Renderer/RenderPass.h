#pragma once

#include "Frost/Renderer/Framebuffer.h"

namespace Frost
{

	struct RenderPassSpecification
	{
		RenderPassSpecification(const std::initializer_list<FramebufferSpecification>& framebuffersSpec)
			: FramebuffersSpecification(framebuffersSpec) {}

		std::vector<FramebufferSpecification> FramebuffersSpecification;
	};

	class RenderPass
	{
	public:
		virtual ~RenderPass() {}

		// TODO: renderpasses should have ownership over framebuffers
		virtual Ref<Framebuffer> GetFramebuffer() = 0;
		virtual void Destroy() = 0;

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		static Ref<RenderPass> Create(const FramebufferSpecification& framebufferSpecs);
	};

}