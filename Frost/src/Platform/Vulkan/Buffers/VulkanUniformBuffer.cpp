#include "frostpch.h"
#include "VulkanUniformBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"

//#include "VulkanBufferAllocator.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Frost
{

	VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size)
	{
		m_BufferSize = size;

		std::vector<BufferType> usages;
		usages.push_back(BufferType::Uniform);
		usages.push_back(BufferType::AccelerationStructureReadOnly);


		VulkanBufferAllocator::CreateBuffer(m_BufferSize,
											usages,
											VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
											m_UniformBuffer, m_UniformBufferMemory);

		VulkanContext::SetStructDebugName("UniformBuffer", VK_OBJECT_TYPE_BUFFER, m_UniformBuffer);


	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
	}

	void VulkanUniformBuffer::SetData(void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		//void* copyData;
		//vkMapMemory(device, m_UniformBufferMemory, 0, m_BufferSize, 0, &copyData);
		//memcpy(copyData, data, m_BufferSize);
		//vkUnmapMemory(device, m_UniformBufferMemory);

		void* copyData;
		VulkanBufferAllocator::BindBuffer(m_UniformBuffer, m_UniformBufferMemory, &copyData);
		memcpy(copyData, data, (size_t)m_BufferSize);
		VulkanBufferAllocator::UnbindBuffer(m_UniformBufferMemory);


	}

	void VulkanUniformBuffer::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		//vkDestroyBuffer(device, m_UniformBuffer, nullptr);
		//vkFreeMemory(device, m_UniformBufferMemory, nullptr);
		VulkanBufferAllocator::DeleteBuffer(m_UniformBuffer, m_UniformBufferMemory);

	}

}