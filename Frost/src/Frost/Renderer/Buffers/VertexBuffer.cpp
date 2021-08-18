#include "frostpch.h"
#include "VertexBuffer.h"

#include "Frost/Renderer/RendererAPI.h"
#include "Platform/Vulkan/Buffers/VulkanVertexBuffer.h"

namespace Frost
{

	Ref<VertexBuffer> VertexBuffer::Create(void* verticies, uint32_t size)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanVertexBuffer>(verticies, size);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}