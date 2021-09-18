#pragma once

#include "Frost/Renderer/Framebuffer.h"
#include "Frost/Renderer/RenderPass.h"
#include "Frost/Platform/Vulkan/Vulkan.h"

struct GLFWwindow;
namespace Frost
{

	class VulkanSwapChain
	{
	public:
		VulkanSwapChain(GLFWwindow* window);
		virtual ~VulkanSwapChain();

		uint32_t GetWidth() { return m_SwapChainExtent.width; }
		uint32_t GetHeight() { return m_SwapChainExtent.height; }
		VkFormat GetImageFormat() { return m_ColorFormat.format; }
		uint32_t GetImageCount() { return (uint32_t)m_SwapChainImages.size(); }

		VkFramebuffer GetVulkanFramebuffer() const { return m_Framebuffer; }
		VkRenderPass GetRenderPass() const { return m_RenderPass; }

		VkSwapchainKHR GetVulkanSwapChain() const { return m_SwapChain; }
		VkSurfaceKHR GetSurface() const { return m_Surface; }

		void BeginFrame(VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex);
		void BeginRenderPass();

		void Present(VkSemaphore waitSemaphore, uint32_t imageIndex);
		void Resize(uint32_t width, uint32_t height);

		uint32_t GetCurrentFrameIndex() const { return m_CurrentBufferIndex; }
		VkCommandBuffer GetRenderCommandBuffer(uint32_t index)
		{
			FROST_ASSERT(bool(index < (uint32_t)m_RenderCommandBuffer.size()), "Index is invalid!");
			return m_RenderCommandBuffer[index];
		}
		VkCommandBuffer GetComputeCommandBuffer(uint32_t index)
		{
			FROST_ASSERT(bool(index < (uint32_t)m_ComputeCommandBuffer.size()), "Index is invalid!");
			return m_ComputeCommandBuffer[index];
		}

	private:
		void PickPresentQueue();
		void CreateSwapChain();
		void CreateRenderPass();
		void CreateFramebuffer();
		void CreateRenderCommandBuffer();
		void CreateComputeCommandBuffer();
		void Destroy();
		uint32_t AcquireImage(VkSemaphore availableSemaphore);
	private:
		VkSwapchainKHR m_SwapChain = VK_NULL_HANDLE;
		Vector<VkImage> m_SwapChainImages;
		Vector<VkImageView> m_SwapChainImageViews;

		VkSurfaceKHR m_Surface;
		VkFramebuffer m_Framebuffer;
		VkRenderPass m_RenderPass;

		VkExtent2D m_SwapChainExtent;
		VkSurfaceFormatKHR m_ColorFormat;

		VkCommandPool m_RenderCommandPool, m_ComputeCommandPool;
		Vector<VkCommandBuffer> m_RenderCommandBuffer;
		Vector<VkCommandBuffer> m_ComputeCommandBuffer;

		VkQueue m_PresentQueue;
		uint32_t m_PresentQueueIndex;

		uint32_t m_CurrentFrameIndex = 0;
		uint32_t m_CurrentBufferIndex = 0;

		friend class VulkanRenderer;
		friend class VulkanContext;
	};

}