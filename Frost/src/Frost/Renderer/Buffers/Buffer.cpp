#include "frostpch.h"
#include "Buffer.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBuffer.h"

namespace Frost
{

	Ref<Buffer> Buffer::Create(uint64_t size, std::initializer_list<BufferType> types)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanBuffer>(size, types);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Buffer> Buffer::Create(uint64_t size, void* data, std::initializer_list<BufferType> types)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanBuffer>(size, data, types);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}