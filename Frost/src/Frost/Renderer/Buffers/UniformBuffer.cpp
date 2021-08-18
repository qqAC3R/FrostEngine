#include "frostpch.h"
#include "UniformBuffer.h"

#include "Frost/Renderer/RendererAPI.h"
#include "Platform/Vulkan/Buffers/VulkanUniformBuffer.h"

namespace Frost
{

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanUniformBuffer>(size);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}