#pragma once

#include "Frost/Renderer/Buffers/UniformBuffer.h"
#include "VulkanBufferAllocator.h"

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanUniformBuffer : public UniformBuffer
	{
	public:
		VulkanUniformBuffer(uint32_t size);
		virtual ~VulkanUniformBuffer();

		virtual void SetData(void* data) override;

		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		VkDeviceAddress GetVulkanBufferAddress() const { return m_BufferAddress; }
		VkDescriptorBufferInfo& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }
		virtual uint32_t GetBufferSize() const override { return m_BufferSize; }

		virtual void Destroy() override;
	private:
		VkBuffer m_Buffer;
		VkDeviceAddress m_BufferAddress;
		VkDescriptorBufferInfo m_DescriptorInfo;

		Ref<BufferDevice> m_UniformBuffer;
		uint32_t m_BufferSize;
	};

}