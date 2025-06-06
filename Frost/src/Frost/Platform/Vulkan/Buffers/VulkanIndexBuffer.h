#pragma once

#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "VulkanBufferAllocator.h"

#include "Frost/Renderer/Mesh.h"

#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(void* indicies, uint64_t count);
		virtual ~VulkanIndexBuffer();

		virtual uint64_t GetCount() const override { return m_BufferSize / sizeof(Index); }
		virtual uint64_t GetBufferSize() const override { return m_BufferSize; }
		virtual Ref<BufferDevice> GetBuffer() const override { return m_IndexBuffer; }

		virtual void Bind() const override;
		virtual void Unbind() const override {}
		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		VkDeviceAddress GetVulkanBufferAddress() const { return m_BufferAddress; }

		virtual void Destroy() override;
	private:
		VkBuffer m_Buffer;
		VkDeviceAddress m_BufferAddress;

		Ref<BufferDevice> m_IndexBuffer;
		uint64_t m_BufferSize;
	};

}