#include "frostpch.h"
#include "VulkanCommandBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	VulkanCommandBuffer::VulkanCommandBuffer()
	{
	}

	VulkanCommandBuffer::~VulkanCommandBuffer()
	{

	}

	void VulkanCommandBuffer::Begin()
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		FROST_VKCHECK(vkBeginCommandBuffer(m_CmdBuf, &beginInfo), "Failed to begin recording command buffer!");

	}

	void VulkanCommandBuffer::End()
	{
		FROST_VKCHECK(vkEndCommandBuffer(m_CmdBuf), "Failed to record command buffer!");
	}


	void VulkanCommandBuffer::Allocate(VkCommandPool cmdPool)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkResetCommandPool(device, cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.commandPool = cmdPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandBufferCount = 1;

		FROST_VKCHECK(vkAllocateCommandBuffers(device, &allocInfo, &m_CmdBuf), "Failed to allocate command buffers!");

		m_CachedCmdPool = cmdPool;


	}

	void VulkanCommandBuffer::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkFreeCommandBuffers(device, m_CachedCmdPool, 1, &m_CmdBuf);
	}




	VulkanCommandBuffer VulkanCommandBuffer::BeginSingleTimeCommands(VulkanCommandPool& cmdPool)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VulkanCommandBuffer cmdBuf;
		cmdBuf.Allocate(cmdPool.GetCommandPool());

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(cmdBuf.GetCommandBuffer(), &beginInfo);

		return cmdBuf;
	}

	void VulkanCommandBuffer::EndSingleTimeCommands(VulkanCommandBuffer& cmdBuf)
	{
		VkQueue graphicsQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Queue;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		
		VkCommandPool cmdPool = cmdBuf.m_CachedCmdPool;
		VkCommandBuffer currentCmdBuf = cmdBuf.m_CmdBuf;

		vkEndCommandBuffer(currentCmdBuf);



		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &currentCmdBuf;
		submitInfo.commandBufferCount = 1;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		VkResult result = vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, cmdPool, 1, &currentCmdBuf);
	}

}