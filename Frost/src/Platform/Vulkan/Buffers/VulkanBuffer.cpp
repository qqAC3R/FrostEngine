#include "frostpch.h"
#include "VulkanBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanShader.h"

//#include "VulkanBufferAllocator.h"

//#define VMA_IMPLEMENTATION
//#include <vk_mem_alloc.h>

namespace Frost
{

	namespace Utils
	{
		VkDescriptorType FromBufferTypeToDescriptorType(std::vector<BufferType> types)
		{
			for (auto type : types)
			{
				switch (type)
				{
					case BufferType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
					case BufferType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				}

			}

			FROST_ASSERT(false, "");
			return VkDescriptorType();
		}

	}

	VulkanBuffer::VulkanBuffer(uint32_t size, std::initializer_list<BufferType> types)
	{
		m_BufferSize = size;

		// Every buffer MUST be accesed by the shader
		std::vector<BufferType> usages;
		for (auto& type : types)
		{
			usages.push_back(type);
		}
		usages.push_back(BufferType::ShaderAddress);
		
		VulkanBufferAllocator::CreateBuffer(m_BufferSize,
											usages,
											VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
											m_Buffer, m_BufferMemory);

		VulkanContext::SetStructDebugName("Buffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);

	}


	VulkanBuffer::VulkanBuffer(void* data, uint32_t size, std::initializer_list<BufferType> types)
		: m_Types(types)
	{
		m_BufferSize = size;



		std::vector<BufferType> usages;
		for (auto& type : types)
		{
			usages.push_back(type);
		}

		// Every buffer MUST be accesed by the shader (BufferType::ShaderAddress)
		usages.push_back(BufferType::ShaderAddress);


		VulkanBufferAllocator::CreateBuffer(size,
											usages,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
											m_Buffer, m_BufferMemory, data);

		VulkanContext::SetStructDebugName("Buffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);

	}

	VulkanBuffer::~VulkanBuffer()
	{
	}

	void VulkanBuffer::SetData(void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		void* copyData;
		VulkanBufferAllocator::BindBuffer(m_Buffer, m_BufferMemory, &copyData);
		memcpy(copyData, data, (size_t)m_BufferSize);
		VulkanBufferAllocator::UnbindBuffer(m_BufferMemory);


	}

	void VulkanBuffer::SetData(uint32_t size, void* data)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		void* copyData;
		VulkanBufferAllocator::BindBuffer(m_Buffer, m_BufferMemory, &copyData);
		memcpy(copyData, data, (size_t)size);
		VulkanBufferAllocator::UnbindBuffer(m_BufferMemory);

	}

	void VulkanBuffer::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VulkanBufferAllocator::DeleteBuffer(m_Buffer, m_BufferMemory);
	}

}