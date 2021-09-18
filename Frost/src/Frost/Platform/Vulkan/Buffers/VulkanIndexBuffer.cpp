#include "frostpch.h"
#include "VulkanIndexBuffer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBuffer.h"

namespace Frost
{

	VulkanIndexBuffer::VulkanIndexBuffer(void* indicies, uint32_t count)
		: m_BufferSize(count)
	{

		m_IndexBuffer = Buffer::Create(m_BufferSize, indicies, { BufferType::Index, BufferType::AccelerationStructureReadOnly });

		// Getting the buffer and buffer address
		Ref<VulkanBuffer> indexBuffer = m_IndexBuffer.As<VulkanBuffer>();
		m_BufferAddress = indexBuffer->GetVulkanBufferAddress();
		m_Buffer = indexBuffer->GetVulkanBuffer();

		VulkanContext::SetStructDebugName("IndexBuffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
	}

	void VulkanIndexBuffer::Bind() const
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		vkCmdBindIndexBuffer(cmdBuf, m_Buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	void VulkanIndexBuffer::Destroy()
	{
		m_IndexBuffer->Destroy();
	}

}