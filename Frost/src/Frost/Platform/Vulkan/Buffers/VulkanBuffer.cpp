#include "frostpch.h"
#include "VulkanBuffer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"

namespace Frost
{

	VulkanBuffer::VulkanBuffer(uint64_t size, std::initializer_list<BufferType> types)
		: m_BufferData{ nullptr, size }
	{
		// Every buffer MUST be accesed by the shader
		std::vector<BufferType> usages;
		for (auto& type : types)
			usages.push_back(type);
		usages.push_back(BufferType::ShaderAddress);
		

		VulkanAllocator::AllocateBuffer(size, usages, MemoryUsage::CPU_AND_GPU, m_Buffer, m_BufferMemory);
		VulkanContext::SetStructDebugName("Buffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
		GetBufferAddress();
		UpdateDescriptor();
	}


	VulkanBuffer::VulkanBuffer(uint64_t size, void* data, std::initializer_list<BufferType> types)
		: m_Types(types), m_BufferData{ data, size }
	{
		std::vector<BufferType> usages;
		for (auto& type : types)
			usages.push_back(type);

		// Every buffer MUST be accesed by the shader (BufferType::ShaderAddress)
		usages.push_back(BufferType::ShaderAddress);

		VulkanAllocator::AllocateBuffer(size, usages, m_Buffer, m_BufferMemory, data);
		VulkanContext::SetStructDebugName("Buffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
		GetBufferAddress();
		UpdateDescriptor();
	}

	VulkanBuffer::~VulkanBuffer()
	{
	}

	void VulkanBuffer::SetData(void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		void* copyData;
		VulkanAllocator::BindBuffer(m_Buffer, m_BufferMemory, &copyData);
		memcpy(copyData, data, m_BufferData.Size);
		VulkanAllocator::UnbindBuffer(m_BufferMemory);
	}

	void VulkanBuffer::SetData(uint64_t size, void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		void* copyData;
		VulkanAllocator::BindBuffer(m_Buffer, m_BufferMemory, &copyData);
		memcpy(copyData, data, size);
		VulkanAllocator::UnbindBuffer(m_BufferMemory);
	}

	void VulkanBuffer::GetBufferAddress()
	{
		// Getting the buffer address
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		bufferInfo.buffer = m_Buffer;
		m_BufferAddress = vkGetBufferDeviceAddress(device, &bufferInfo);
	}

	void VulkanBuffer::UpdateDescriptor()
	{
		m_DescriptorInfo.buffer = m_Buffer;
		m_DescriptorInfo.offset = 0;
		m_DescriptorInfo.range = m_BufferData.Size;
	}

	void VulkanBuffer::Destroy()
	{
		if (m_Buffer == VK_NULL_HANDLE) return;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanAllocator::DeleteBuffer(m_Buffer, m_BufferMemory);
		m_Buffer = VK_NULL_HANDLE;
	}

}