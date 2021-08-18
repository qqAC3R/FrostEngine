#include "frostpch.h"
#include "VulkanVertexBuffer.h"

//#include "VulkanBufferAllocator.h"
#include "Frost/Renderer/Buffers/BufferLayout.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanVertexBuffer::VulkanVertexBuffer(void* verticies, uint32_t size)
	{
		m_BufferSize = size;

		std::vector<BufferType> usages;
		usages.push_back(BufferType::Vertex);
		usages.push_back(BufferType::AccelerationStructureReadOnly);
		usages.push_back(BufferType::ShaderAddress);


		VulkanBufferAllocator::CreateBuffer(size,
											usages,
											VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
											m_VertexBuffer, m_VertexBufferMemory, verticies);

		VulkanContext::SetStructDebugName("VertexBuffer", VK_OBJECT_TYPE_BUFFER, m_VertexBuffer);



	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
	}

	void VulkanVertexBuffer::Bind(void* cmdBuf) const
	{
		uint64_t offset = 0;
		VkCommandBuffer commandBuffer = (VkCommandBuffer)cmdBuf;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VertexBuffer, &offset);
	}

	BufferLayout VulkanVertexBuffer::GetLayout() const
	{
		return BufferLayout();
	}

	void VulkanVertexBuffer::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		//vkDestroyBuffer(device, m_VertexBuffer, nullptr);
		//vkFreeMemory(device, m_VertexBufferMemory, nullptr);
		VulkanBufferAllocator::DeleteBuffer(m_VertexBuffer, m_VertexBufferMemory);

	}

}