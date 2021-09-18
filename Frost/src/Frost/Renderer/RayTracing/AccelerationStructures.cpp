#include "frostpch.h"
#include "AccelerationStructures.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

namespace Frost
{
	Ref<BottomLevelAccelerationStructure> BottomLevelAccelerationStructure::Create(const Ref<VertexBuffer>& vertexBuffer,
																				   const Ref<IndexBuffer>& indexBuffer)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "OpenGL::API doesn't support RayTracing!");
			case RendererAPI::API::Vulkan: return Ref<VulkanBottomLevelAccelerationStructure>::Create(vertexBuffer, indexBuffer);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<TopLevelAccelertionStructure> TopLevelAccelertionStructure::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "OpenGL::API doesn't support RayTracing!");
			case RendererAPI::API::Vulkan: return Ref<VulkanTopLevelAccelertionStructure>::Create();
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}