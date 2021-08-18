#pragma once

#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "VulkanBufferAllocator.h"

#include "Frost/Renderer/Mesh.h"

#include <vulkan/vulkan.hpp>

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanIndexBuffer : public IndexBuffer
	{
	public:
		VulkanIndexBuffer(void* indicies, uint32_t count);
		virtual ~VulkanIndexBuffer();

		virtual uint32_t GetCount() const override { return m_BufferSize / sizeof(Index); }
		virtual uint32_t GetBufferSize() const override { return m_BufferSize; }

		virtual VkBuffer GetVulkanBuffer() const override { return m_IndexBuffer; }
		virtual void Bind(void* cmdBuf) const override;

		virtual void Destroy() override;

	private:
		VkBuffer m_IndexBuffer;
		VulkanMemoryInfo m_IndexBufferMemory;

		uint32_t m_BufferSize;


	public:
		// Fuctions that are useless
		virtual void Bind() const override {}
		virtual void Unbind() const override {}

	};

}