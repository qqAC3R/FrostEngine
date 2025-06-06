#include "frostpch.h"
#include "RayTracingPipeline.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanRayTracingPipeline.h"

namespace Frost
{
	Ref<RayTracingPipeline> RayTracingPipeline::Create(const RayTracingPipeline::CreateInfo& pipelineCreateInfo)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return Ref<VulkanRayTracingPipeline>::Create(pipelineCreateInfo);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}