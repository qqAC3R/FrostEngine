#include "frostpch.h"
#include "Buffer.h"

#include "Frost/Renderer/RendererAPI.h"
#include "Platform/Vulkan/Buffers/VulkanBuffer.h"

namespace Frost
{

	Ref<Buffer> Buffer::Create(uint32_t size, std::initializer_list<BufferType> types)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanBuffer>(size, types);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Buffer> Buffer::Create(void* data, uint32_t size, std::initializer_list<BufferType> types)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanBuffer>(data, size, types);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}