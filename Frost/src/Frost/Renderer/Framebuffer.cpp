#include "frostpch.h"
#include "Framebuffer.h"

#include "Frost/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"

namespace Frost
{

	Ref<Framebuffer> Framebuffer::Create(void* renderPass, const FramebufferSpecification& spec)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanFramebuffer>(renderPass, spec);
		}
		
		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Framebuffer> Framebuffer::Create(const FramebufferSpecification& spec)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::OpenGL: return nullptr;
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}