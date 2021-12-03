#include "frostpch.h"
#include "GraphicsContext.h"

#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

namespace Frost
{
	Scope<GraphicsContext> GraphicsContext::Create(GLFWwindow* window)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateScope<VulkanContext>(window);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}
}