#pragma once

#include "Frost/Renderer/Framebuffer.h"
#include "Frost/Renderer/RenderPass.h"

#include <vulkan/vulkan.hpp>

namespace Frost
{

	class VulkanSwapChain
	{
	public:
		VulkanSwapChain();
		virtual ~VulkanSwapChain();
		
		VkExtent2D GetExtent() { return m_Extent; }
		VkFormat   GetImageFormat() { return m_SwapChainFormat; }
		VkSwapchainKHR GetVulkanSwapChain() { return m_SwapChain; }
		Vector<VkImage> GetSwapChainImages() { return m_SwapChainImages; }

		Ref<Image2D> GetFramebufferAttachment(uint32_t slot = 0) { return m_FramebufferAttachments[slot]; }
		VkFramebuffer GetVulkanFramebuffer() { return m_Framebuffer; }

	private:
		void CreateSwapChain();
		void CreateFramebuffer(Ref<RenderPass> renderPass);
		void Destroy();

	private:
		VkSwapchainKHR m_SwapChain;
		Vector<VkImage> m_SwapChainImages;

		Vector<Ref<Image2D>> m_FramebufferAttachments;
		VkFramebuffer m_Framebuffer;

		VkExtent2D m_Extent;
		VkFormat m_SwapChainFormat;

		friend class VulkanRenderer;
		friend class VulkanContext;

	};

}