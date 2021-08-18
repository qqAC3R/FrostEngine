#pragma once

#include "Frost/Renderer/Buffers/UniformBuffer.h"
//#include "Platform/Vulkan/VulkanDescriptor.h"

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

		virtual VkBuffer GetVulkanBuffer() const override { return m_UniformBuffer; }
		virtual uint32_t GetBufferSize() const override { return m_BufferSize; }

		virtual void Destroy() override;

	private:
		VkBuffer m_UniformBuffer;
		VulkanMemoryInfo m_UniformBufferMemory;

		uint32_t m_BufferSize;

		// For descriptor
		//VkDescriptorBufferInfo m_DescriptorBufferInfo;
		//VulkanDescriptorLayout m_DescriptorLayout;

	};

}