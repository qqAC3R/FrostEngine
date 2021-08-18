#pragma once

//#include <vulkan/vulkan.hpp>

#include "Platform/Vulkan/VulkanContext.h"

namespace Frost
{

	class VulkanSemaphore
	{
	public:
		VulkanSemaphore() = default;
		virtual ~VulkanSemaphore() {}

		void Create()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkSemaphoreCreateInfo semaphoreInfo{};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			FROST_VKCHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_Semaphore), "Semaphore not created");
			VulkanContext::SetStructDebugName("Semaphore", VK_OBJECT_TYPE_SEMAPHORE, m_Semaphore);
		}

		void Destroy()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			vkDestroySemaphore(device, m_Semaphore, nullptr);
		}

		inline const VkSemaphore& GetSemaphore() { return m_Semaphore; }

	private:
		VkSemaphore m_Semaphore;
	};

	class VulkanFence
	{
	public:
		VulkanFence() = default;
		virtual ~VulkanFence() {}

		void Create()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			VkFenceCreateInfo fenceInfo{};
			fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			FROST_VKCHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_Fence), "Fence not created!");
			VulkanContext::SetStructDebugName("Fence", VK_OBJECT_TYPE_FENCE, m_Fence);
		}

		void Destroy()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkDestroyFence(device, m_Fence, nullptr);
		}

		void Wait()
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			vkWaitForFences(device, 1, &m_Fence, VK_TRUE, UINT64_MAX);
		}

		inline const VkFence& GetFence() { return m_Fence; }

	private:
		VkFence m_Fence;
	};
	
}