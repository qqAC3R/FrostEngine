#include "frostpch.h"
#include "PipelineCompute.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"

namespace Frost
{

	Ref<ComputePipeline> ComputePipeline::Create(const ComputePipeline::CreateInfo& createInfo)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanComputePipeline>(createInfo);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}