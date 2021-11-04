#pragma once

#include "Frost/Renderer/Buffers/BufferDevice.h"

#include "VulkanBufferAllocator.h"

namespace Frost
{
	enum class ShaderType;
	struct VulkanMemoryInfo;

	class VulkanBufferDevice : public BufferDevice
	{
	public:
		VulkanBufferDevice(uint64_t size, std::initializer_list<BufferUsage> types);
		VulkanBufferDevice(uint64_t size, void* data, std::initializer_list<BufferUsage> types);
		virtual ~VulkanBufferDevice();

		virtual uint64_t GetBufferSize() const override { return m_BufferData.Size; }
		virtual BufferData GetBufferData() const override { return m_BufferData; }
		virtual void SetData(uint64_t size, void* data) override;
		virtual void SetData(void* data) override;

		VkBuffer GetVulkanBuffer() const { return m_Buffer; }
		VkDeviceAddress GetVulkanBufferAddress() const { return m_BufferAddress; }
		VkDescriptorBufferInfo& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }

		virtual void Destroy() override;
	private:
		void GetBufferAddress();
		void UpdateDescriptor();
	private:
		VkBuffer m_Buffer;
		VulkanMemoryInfo m_BufferMemory;
		VkDeviceAddress m_BufferAddress;

		VkDescriptorBufferInfo m_DescriptorInfo;

		BufferData m_BufferData;
		Vector<BufferUsage> m_Types;
	public:
		// Functions that are useless
		virtual void Bind() const override {}
		virtual void Unbind() const override {}

	};

}