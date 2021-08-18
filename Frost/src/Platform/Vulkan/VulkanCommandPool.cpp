#include "frostpch.h"
#include "VulkanCommandPool.h"

#include "Platform/Vulkan/VulkanContext.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Frost
{

	VulkanCommandPool::VulkanCommandPool()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t graphicsQueueIndex = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Index;

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = graphicsQueueIndex;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to create command pool!");
		}
		
		VulkanContext::SetStructDebugName("CommandPool", VK_OBJECT_TYPE_COMMAND_POOL, m_CommandPool);


	}

	VulkanCommandPool::~VulkanCommandPool()
	{
		//VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		//if(m_CommandPool)
		//	vkDestroyCommandPool(device, m_CommandPool, nullptr);
	}

	//void VulkanCommandPool::CreateCommandPool()
	//{
	//	
	//}

	void VulkanCommandPool::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyCommandPool(device, m_CommandPool, nullptr);
	}

	VulkanCommandBuffer VulkanCommandPool::CreateCommandBuffer(bool beginRecording)
	{
		VulkanCommandBuffer cmdBuf;
		cmdBuf.Allocate(m_CommandPool);

		if (!beginRecording) return cmdBuf;

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		FROST_VKCHECK(vkBeginCommandBuffer(cmdBuf.GetCommandBuffer(), &beginInfo), "Failed to begin recording command buffer!");

		return cmdBuf;

	}

}