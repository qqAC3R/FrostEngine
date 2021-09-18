#include "frostpch.h"
#include "Pipeline.h"

#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/VulkanPipeline.h"

namespace Frost
{

	Ref<Pipeline> Pipeline::Create(Pipeline::CreateInfo& createInfo)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanPipeline>(createInfo);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}