#include "frostpch.h"
#include "VertexBuffer.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"

namespace Frost
{

	Ref<VertexBuffer> VertexBuffer::Create(void* verticies, uint64_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanVertexBuffer>(verticies, size);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}