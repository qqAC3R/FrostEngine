#include "frostpch.h"
#include "VulkanIndexBuffer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"

namespace Frost
{
	VulkanIndexBuffer::VulkanIndexBuffer(void* indicies, uint32_t count)
		: m_BufferSize(count)
	{

		m_IndexBuffer = BufferDevice::Create(m_BufferSize, indicies, { BufferUsage::Index, BufferUsage::AccelerationStructureReadOnly });

		// Getting the buffer and buffer address
		Ref<VulkanBufferDevice> indexBuffer = m_IndexBuffer.As<VulkanBufferDevice>();
		m_BufferAddress = indexBuffer->GetVulkanBufferAddress();
		m_Buffer = indexBuffer->GetVulkanBuffer();

		VulkanContext::SetStructDebugName("IndexBuffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		Destroy();
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