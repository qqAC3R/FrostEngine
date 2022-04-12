#include "frostpch.h"
#include "ImGuiLayer.h"

#include "Frost/Core/Engine.h"

#include "Frost/Renderer/Renderer.h"
#include "ImGuiVulkanLayer.h"

namespace Frost
{
	ImGuiLayer* ImGuiLayer::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT_MSG("Renderer::API::None is not supported");
			case RendererAPI::API::Vulkan: return new VulkanImGuiLayer();
		}
		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}
}