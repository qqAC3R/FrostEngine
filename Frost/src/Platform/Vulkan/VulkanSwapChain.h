#pragma once

#include <vulkan/vulkan.hpp>

namespace Frost
{

	class VulkanSwapChain
	{
	public:
		VulkanSwapChain();
		virtual ~VulkanSwapChain();
		
		VkExtent2D GetExtent();
		VkFormat GetImageFormat();
		VkSwapchainKHR GetVulkanSwapChain() { return m_SwapChain; }
		std::vector<VkImage> GetSwapChainImages() { return m_SwapChainImages; }

	private:
		void CreateSwapChain();
		void Destroy();

	private:
		VkSwapchainKHR m_SwapChain;
		std::vector<VkImage> m_SwapChainImages;

		friend class VulkanRenderer;
		friend class VulkanContext;

	};

}