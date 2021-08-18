#include "frostpch.h"
#include "VulkanIndexBuffer.h"

//#include "VulkanBufferAllocator.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanIndexBuffer::VulkanIndexBuffer(void* indicies, uint32_t count)
	{
		m_BufferSize = count * sizeof(Index);

		std::vector<BufferType> usages;
		usages.push_back(BufferType::Index);
		usages.push_back(BufferType::AccelerationStructureReadOnly);
		usages.push_back(BufferType::ShaderAddress);


		VulkanBufferAllocator::CreateBuffer(m_BufferSize,
											usages,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
											m_IndexBuffer, m_IndexBufferMemory, indicies);

		VulkanContext::SetStructDebugName("IndexBuffer", VK_OBJECT_TYPE_BUFFER, m_IndexBuffer);


	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
	}

	void VulkanIndexBuffer::Bind(void* cmdBuf) const
	{
		vkCmdBindIndexBuffer((VkCommandBuffer)cmdBuf, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
	}

	void VulkanIndexBuffer::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VulkanBufferAllocator::DeleteBuffer(m_IndexBuffer, m_IndexBufferMemory);
		//vkDestroyBuffer(device, m_IndexBuffer, nullptr);
		//vkFreeMemory(device, m_IndexBufferMemory, nullptr);
	}

}