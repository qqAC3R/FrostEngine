#include "frostpch.h"
#include "VulkanUniformBuffer.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"

namespace Frost
{

	VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size)
		: m_BufferSize(size)
	{
		m_UniformBuffer = BufferDevice::Create(size, { BufferUsage::Uniform, BufferUsage::AccelerationStructureReadOnly });

		// Getting the buffer and buffer address
		Ref<VulkanBufferDevice> uniformBuffer = m_UniformBuffer.As<VulkanBufferDevice>();
		m_Buffer = uniformBuffer->GetVulkanBuffer();
		m_BufferAddress = uniformBuffer->GetVulkanBufferAddress();
		m_DescriptorInfo = uniformBuffer->GetVulkanDescriptorInfo();

		VulkanContext::SetStructDebugName("UniformBuffer", VK_OBJECT_TYPE_BUFFER, m_Buffer);
	}

	VulkanUniformBuffer::~VulkanUniformBuffer()
	{
		Destroy();
	}

	void VulkanUniformBuffer::SetData(void* data)
	{
		m_UniformBuffer->SetData(data);
	}

	void VulkanUniformBuffer::Destroy()
	{
		m_UniformBuffer->Destroy();
	}

}