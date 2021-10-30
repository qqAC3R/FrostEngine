#include "frostpch.h"
#include "IndexBuffer.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanIndexBuffer.h"

namespace Frost
{

	Ref<IndexBuffer> IndexBuffer::Create(void* indicies, uint32_t count)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanIndexBuffer>(indicies, count);
		}
		
		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}