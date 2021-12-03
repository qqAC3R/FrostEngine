#include "frostpch.h"
#include "SceneEnvironment.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanSceneEnvironment.h"

namespace Frost
{
	Ref<SceneEnvironment> SceneEnvironment::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanSceneEnvironment>();
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}
}