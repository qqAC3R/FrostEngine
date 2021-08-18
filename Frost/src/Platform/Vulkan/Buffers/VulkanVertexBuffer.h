#pragma once

#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "VulkanBufferAllocator.h"

#include <vulkan/vulkan.hpp>

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanVertexBuffer : public VertexBuffer
	{
	public:
		VulkanVertexBuffer(void* verticies, uint32_t size);
		virtual ~VulkanVertexBuffer();

		virtual uint32_t GetBufferSize() const { return m_BufferSize; }

		virtual VkBuffer GetVulkanBuffer() const override { return m_VertexBuffer; }
		virtual void Bind(void* cmdBuf) const override;

		virtual void Destroy() override;
		
	private:

		VkBuffer m_VertexBuffer;
		VulkanMemoryInfo m_VertexBufferMemory;
		//VkDeviceMemory m_VertexBufferMemory;


		uint32_t m_BufferSize;


	public:
		// Functions that are useless
		virtual void Bind() const override {} // NULL
		virtual void Unbind() const override {} // NULL
		virtual BufferLayout GetLayout() const override; // NULL
		virtual void SetLayout(const BufferLayout& layout) override {} // NULL
	};

}