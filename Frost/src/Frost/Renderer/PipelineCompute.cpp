#include "frostpch.h"
#include "PipelineCompute.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"

namespace Frost
{

	Ref<ComputePipeline> ComputePipeline::Create(ComputePipeline::CreateInfo& createInfo)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanComputePipeline>(createInfo);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}