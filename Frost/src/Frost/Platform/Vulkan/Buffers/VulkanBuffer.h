#pragma once

#include "Frost/Renderer/Buffers/Buffer.h"

#include "VulkanBufferAllocator.h"

namespace Frost
{
	enum class ShaderType;
	struct VulkanMemoryInfo;

	class VulkanBuffer : public Buffer
	{
	public:
		VulkanBuffer(uint32_t size, std::initializer_list<BufferType> types);
		VulkanBuffer(uint32_t size, void* data, std::initializer_list<BufferType> types);
		virtual ~VulkanBuffer();

		virtual uint32_t GetBufferSize() const override { return m_BufferData.Size; }
		virtual BufferData GetBufferData() const override { return m_BufferData; }
		virtual void SetData(uint32_t size, void* data) override;
		virtual void SetData(void* data) override;

		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		VkDeviceAddress GetVulkanBufferAddress() const { return m_BufferAddress; }

		virtual void Destroy() override;

	private:
		void GetBufferAddress();
	private:
		VkBuffer m_Buffer;
		VulkanMemoryInfo m_BufferMemory;
		VkDeviceAddress m_BufferAddress;

		BufferData m_BufferData;

		Vector<BufferType> m_Types;
	public:
		// Functions that are useless
		virtual void Bind() const override {}
		virtual void Unbind() const override {}

	};

}