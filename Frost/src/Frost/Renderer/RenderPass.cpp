#include "frostpch.h"
#include "RenderPass.h"

#include "Frost/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanRenderPass.h"

namespace Frost
{

	Ref<RenderPass> RenderPass::Create(const FramebufferSpecification& framebufferSpecs)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanRenderPass>(framebufferSpecs);
			case RendererAPI::API::OpenGL: return nullptr;
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}