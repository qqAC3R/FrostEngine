#include "frostpch.h"
#include "VulkanVertexBuffer.h"

#include "Frost/Renderer/Buffers/BufferLayout.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBuffer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanVertexBuffer::VulkanVertexBuffer(void* verticies, uint64_t size)
		: m_BufferSize(size)
	{
		m_VertexBuffer = Buffer::Create(size, verticies, { BufferType::Vertex, BufferType::AccelerationStructureReadOnly });
	
		// Getting the buffer and buffer address
		Ref<VulkanBuffer> vertexBuffer = m_VertexBuffer.As<VulkanBuffer>();
		m_BufferAddress = vertexBuffer->GetVulkanBufferAddress();
		m_Buffer = vertexBuffer->GetVulkanBuffer();

		VulkanContext::SetStructDebugName("VertexBuffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
	}

	VulkanVertexBuffer::~VulkanVertexBuffer()
	{
		Destroy();
	}

	void VulkanVertexBuffer::Bind() const
	{
		uint64_t offset = 0;
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m_Buffer, &offset);
	}

	BufferLayout VulkanVertexBuffer::GetLayout() const
	{
		return BufferLayout();
	}

	void VulkanVertexBuffer::Destroy()
	{
		m_VertexBuffer->Destroy();
	}

}