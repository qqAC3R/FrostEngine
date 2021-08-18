#include "frostpch.h"
#include "GraphicsContext.h"

#include "Frost/Renderer/RendererAPI.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{



	Scope<GraphicsContext> GraphicsContext::Create(GLFWwindow* window)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateScope<VulkanContext>(window);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}