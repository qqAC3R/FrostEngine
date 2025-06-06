#include "frostpch.h"
#include "Framebuffer.h"

#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"

namespace Frost
{
	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return Ref<VulkanFramebuffer>::Create(spec);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}
}