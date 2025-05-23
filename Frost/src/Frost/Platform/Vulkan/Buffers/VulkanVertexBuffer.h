#pragma once

#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "VulkanBufferAllocator.h"

#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(void* verticies, uint64_t size);
		virtual ~VulkanVertexBuffer();

		virtual void Bind() const override;
		virtual void Unbind() const override {}

		virtual uint64_t GetBufferSize() const { return m_BufferSize; }
		virtual BufferLayout GetLayout() const override;
		virtual void SetLayout(const BufferLayout& layout) override {}
		
		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		VkDeviceAddress GetVulkanBufferAddress() const { return m_BufferAddress; }
		virtual Ref<BufferDevice> GetBuffer() const override { return m_VertexBuffer; }

		virtual void Destroy() override;
	private:
		VkBuffer m_Buffer;
		VkDeviceAddress m_BufferAddress;
		
		Ref<BufferDevice> m_VertexBuffer;
		uint64_t m_BufferSize;
	};

}