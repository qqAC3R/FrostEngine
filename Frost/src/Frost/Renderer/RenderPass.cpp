#include "frostpch.h"
#include "RenderPass.h"

#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/VulkanRenderPass.h"

namespace Frost
{

	Ref<RenderPass> RenderPass::Create(const RenderPassSpecification& renderPassSpecs)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanRenderPass>(renderPassSpecs);
			case RendererAPI::API::OpenGL: return nullptr;
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}