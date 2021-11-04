#include "frostpch.h"
#include "VulkanVertexBuffer.h"

#include "Frost/Renderer/Buffers/BufferLayout.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanVertexBuffer::VulkanVertexBuffer(void* verticies, uint64_t size)
		: m_BufferSize(size)
	{
		m_VertexBuffer = BufferDevice::Create(size, verticies, { BufferUsage::Vertex, BufferUsage::AccelerationStructureReadOnly });
	
		// Getting the buffer and buffer address
		Ref<VulkanBufferDevice> vertexBuffer = m_VertexBuffer.As<VulkanBufferDevice>();
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