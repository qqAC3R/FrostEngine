#include "frostpch.h"
#include "VulkanBufferDevice.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"

namespace Frost
{

	VulkanBufferDevice::VulkanBufferDevice(uint64_t size, std::initializer_list<BufferUsage> types)
		: m_BufferData{ nullptr, size }
	{
		// Every buffer MUST be accesed by the shader
		std::vector<BufferUsage> usages;
		for (auto& type : types)
		{
			if (type != BufferUsage::AccelerationStructure &&
				type != BufferUsage::AccelerationStructureReadOnly &&
				type != BufferUsage::ShaderBindingTable)
			{
				usages.push_back(type);
			}

#if FROST_SUPPORT_RAY_TRACING
			if (type == BufferUsage::AccelerationStructure ||
				type == BufferUsage::AccelerationStructureReadOnly ||
				type == BufferUsage::ShaderBindingTable)
			{
				usages.push_back(type);
			}
#endif
		}
		usages.push_back(BufferUsage::ShaderAddress);
		
		// CPU_ONLY means that the memory is preferably fast to access by GPU and fast to be mapped by the host (tho it is uncached).
		// This fits the best for uniform/storage buffers that are used everyframe.
		VulkanAllocator::AllocateBuffer(size, usages, MemoryUsage::CPU_TO_GPU, m_Buffer, m_BufferMemory);
		VulkanContext::SetStructDebugName("Buffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
		GetBufferAddress();
		UpdateDescriptor();
	}


	VulkanBufferDevice::VulkanBufferDevice(uint64_t size, void* data, std::initializer_list<BufferUsage> types)
		: m_Types(types), m_BufferData{ data, size }
	{
		std::vector<BufferUsage> usages;
		for (auto& type : types)
		{
			if (type != BufferUsage::AccelerationStructure &&
				type != BufferUsage::AccelerationStructureReadOnly &&
				type != BufferUsage::ShaderBindingTable)
			{
				usages.push_back(type);
			}

#if FROST_SUPPORT_RAY_TRACING
			if (type == BufferUsage::AccelerationStructure ||
				type == BufferUsage::AccelerationStructureReadOnly ||
				type == BufferUsage::ShaderBindingTable)
			{
				usages.push_back(type);
			}
#endif
		}
		// Every buffer MUST be accesed by the shader (BufferType::ShaderAddress)
		usages.push_back(BufferUsage::ShaderAddress);

		VulkanAllocator::AllocateBuffer(size, usages, m_Buffer, m_BufferMemory, data);
		VulkanContext::SetStructDebugName("Buffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
		GetBufferAddress();
		UpdateDescriptor();
	}

	VulkanBufferDevice::~VulkanBufferDevice()
	{
		Destroy();
	}

	void VulkanBufferDevice::SetData(void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		void* copyData;
		VulkanAllocator::BindBuffer(m_Buffer, m_BufferMemory, &copyData);
		memcpy(copyData, data, m_BufferData.Size);
		VulkanAllocator::UnbindBuffer(m_BufferMemory);
	}

	void VulkanBufferDevice::SetData(uint64_t size, void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		void* copyData;
		VulkanAllocator::BindBuffer(m_Buffer, m_BufferMemory, &copyData);
		memcpy(copyData, data, size);
		VulkanAllocator::UnbindBuffer(m_BufferMemory);
	}

	void VulkanBufferDevice::GetBufferAddress()
	{
		// Getting the buffer address
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		bufferInfo.buffer = m_Buffer;
		m_BufferAddress = vkGetBufferDeviceAddress(device, &bufferInfo);
	}

	void VulkanBufferDevice::UpdateDescriptor()
	{
		m_DescriptorInfo.buffer = m_Buffer;
		m_DescriptorInfo.offset = 0;
		m_DescriptorInfo.range = m_BufferData.Size;
	}

	void VulkanBufferDevice::SetMemoryBarrier(VkCommandBuffer cmdBuf,
		VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
		VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
	{
		VkBufferMemoryBarrier memoryBufferBarrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
		memoryBufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		memoryBufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		memoryBufferBarrier.srcAccessMask = srcAccessMask;
		memoryBufferBarrier.dstAccessMask = dstAccessMask;

		memoryBufferBarrier.buffer = m_Buffer;
		memoryBufferBarrier.size = m_DescriptorInfo.range;
		memoryBufferBarrier.offset = 0;

		vkCmdPipelineBarrier(cmdBuf,
			srcStageMask,
			dstStageMask,
			0,
			0, nullptr,
			1, &memoryBufferBarrier,
			0, nullptr
		);
	}

	void VulkanBufferDevice::Destroy()
	{
		if (m_Buffer == VK_NULL_HANDLE) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanAllocator::DeleteBuffer(m_Buffer, m_BufferMemory);
		m_Buffer = VK_NULL_HANDLE;
	}

}