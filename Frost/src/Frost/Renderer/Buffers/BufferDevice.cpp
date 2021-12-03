#include "frostpch.h"
#include "BufferDevice.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"

namespace Frost
{

	Ref<BufferDevice> BufferDevice::Create(uint64_t size, std::initializer_list<BufferUsage> types)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanBufferDevice>(size, types);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	Ref<BufferDevice> BufferDevice::Create(uint64_t size, void* data, std::initializer_list<BufferUsage> types)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanBufferDevice>(size, data, types);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}