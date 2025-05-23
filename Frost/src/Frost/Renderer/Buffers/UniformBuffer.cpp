#include "frostpch.h"
#include "UniformBuffer.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanUniformBuffer.h"

namespace Frost
{

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanUniformBuffer>(size);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}