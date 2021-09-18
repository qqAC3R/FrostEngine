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
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "OpenGL::API doesn't support RayTracing!");
			case RendererAPI::API::Vulkan: return Ref<VulkanRayTracingPipeline>::Create(pipelineCreateInfo);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}