#include "frostpch.h"
#include "AccelerationStructures.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

namespace Frost
{
	Ref<BottomLevelAccelerationStructure> BottomLevelAccelerationStructure::Create(const MeshASInfo& meshInfo)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return Ref<VulkanBottomLevelAccelerationStructure>::Create(meshInfo);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	Ref<TopLevelAccelertionStructure> TopLevelAccelertionStructure::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return Ref<VulkanTopLevelAccelertionStructure>::Create();
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}