#include "frostpch.h"
#include "VulkanSwapChain.h"

#include "Frost/Core/Application.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Core/Engine.h"
#include <GLFW/glfw3.h>

#include "Frost/Renderer/Renderer.h"

namespace Frost
{
	namespace Utils
	{
		static void CreateSurface(VkInstance& instance, GLFWwindow* window, VkSurfaceKHR& surface)
		{
			if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
				FROST_ASSERT(0, "Failed to create window surface!");
		}
	}

	VulkanSwapChain::VulkanSwapChain(GLFWwindow* window)
	{
		VkInstance instance = VulkanContext::GetInstance();
		Utils::CreateSurface(instance, window, m_Surface);
		PickPresentQueue();
		CreateSwapChain();
		CreateRenderPass();
		CreateFramebuffer();
		CreateRenderCommandBuffer();
		CreateComputeCommandBuffer();
	}
	
	VulkanSwapChain::~VulkanSwapChain()
	{
	}

	void VulkanSwapChain::Resize(uint32_t width, uint32_t height)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		m_SwapChainExtent = { width, height };
		m_CurrentFrameIndex = 0;

		for (auto imageView : m_SwapChainImageViews)
			vkDestroyImageView(device, imageView, nullptr);
		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);

		CreateSwapChain();
		CreateFramebuffer();
	}

	void VulkanSwapChain::CreateSwapChain()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

		VkSurfaceCapabilitiesKHR surfaceCapabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &surfaceCapabilities);

		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		
		VkExtent2D swapChainExtent = {};
		if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
		{
			swapChainExtent = surfaceCapabilities.currentExtent;
		}


		// Getting all swapchain formats
		uint32_t availableFormatCount = 0;
		Vector<VkSurfaceFormatKHR> availableFormats;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &availableFormatCount, nullptr);
		availableFormats.resize(availableFormatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &availableFormatCount, availableFormats.data());

		// Selecting the best swapchain format
		VkSurfaceFormatKHR pickedFormat = availableFormats[0]; // Default one is `availableFormats[0]`
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				pickedFormat = availableFormat;
			}
		}


		// Getting all swapchain presentModes
		uint32_t availablePresentModeCount = 0;
		Vector<VkPresentModeKHR> avaialbePresentModes;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &availablePresentModeCount, nullptr);
		avaialbePresentModes.resize(availablePresentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &availablePresentModeCount, avaialbePresentModes.data());

		// Selecting the best swapchain present mode
		VkPresentModeKHR pickedPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Default one
		for (const auto& availablePresentMode : avaialbePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				pickedPresentMode = availablePresentMode;
			}
		}


		VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = pickedFormat.format;
		createInfo.imageColorSpace = pickedFormat.colorSpace;
		createInfo.imageExtent = swapChainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // TODO: Shoud add support for `VK_SHARING_MODE_CONCURRENT` later
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;
		createInfo.preTransform = surfaceCapabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = pickedPresentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = m_SwapChain ? m_SwapChain : VK_NULL_HANDLE;

		FROST_VKCHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_SwapChain));
		VulkanContext::SetStructDebugName("SwapChain", VK_OBJECT_TYPE_SWAPCHAIN_KHR, m_SwapChain);


		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, m_SwapChainImages.data());

		// Setting up the members
		m_ColorFormat = pickedFormat;
		m_SwapChainExtent = swapChainExtent;
	}

	void VulkanSwapChain::CreateRenderPass()
	{
		// Creating here the renderPass because we need it for the framebuffer, for imgui and other stuff.
	   // The rendered image will be passed to this renderpass by rendering a quad with the final texture or viewport and then it will pe presented

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_ColorFormat.format;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;


		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		FROST_VKCHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_RenderPass));

	}

	void VulkanSwapChain::CreateFramebuffer()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Creating the VkImageViews
		m_SwapChainImageViews.resize(m_SwapChainImages.size());
		for (uint32_t i = 0; i < m_SwapChainImages.size(); i++)
		{
			VkImage swapChainImage = m_SwapChainImages[i];

			// Specifying that we are gonna use the imageviews as color attachments
			VkImageViewUsageCreateInfo usageInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO };
			usageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


			VkImageViewCreateInfo createInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			createInfo.pNext = &usageInfo; // using pNext chain for letting vulkan know that this imageView is gonna be used for the imageless framebuffer
			createInfo.image = swapChainImage;
			createInfo.format = m_ColorFormat.format;
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			FROST_VKCHECK(vkCreateImageView(device, &createInfo, nullptr, &m_SwapChainImageViews[i]));
		}


		// Imageless framebuffer (Vulkan Core feature 1.2)
		// Firstly we tell vulkan how we are gonna use the image views
		VkFramebufferAttachmentImageInfo attachmentImageInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO };
		attachmentImageInfo.pNext = nullptr;
		attachmentImageInfo.width = m_SwapChainExtent.width;
		attachmentImageInfo.height = m_SwapChainExtent.height;
		attachmentImageInfo.flags = 0;
		attachmentImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		attachmentImageInfo.layerCount = 1;
		attachmentImageInfo.viewFormatCount = 1;
		attachmentImageInfo.pViewFormats = &m_ColorFormat.format;

		// Reference the struct above here and specify how many attachments we have
		VkFramebufferAttachmentsCreateInfo attachmentCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO };
		attachmentCreateInfo.attachmentImageInfoCount = 1;
		attachmentCreateInfo.pAttachmentImageInfos = &attachmentImageInfo;

		// Creation of the framebuffer is done using the pNext chain and with the flag `VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT`
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.pNext = &attachmentCreateInfo;
		framebufferInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = nullptr;
		framebufferInfo.width = m_SwapChainExtent.width;
		framebufferInfo.height = m_SwapChainExtent.height;
		framebufferInfo.layers = 1;

		FROST_VKCHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_Framebuffer));

		VulkanContext::SetStructDebugName("Framebuffer", VK_OBJECT_TYPE_FRAMEBUFFER, m_Framebuffer);
	}

	void VulkanSwapChain::CreateRenderCommandBuffer()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Creating the render comamnd pool
		VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		cmdPoolInfo.queueFamilyIndex = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Index;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		FROST_VKCHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &m_RenderCommandPool));

		m_RenderCommandBuffer.resize(m_SwapChainImages.size());
		for (uint32_t i = 0; i < Renderer::GetRendererConfig().FramesInFlight; i++)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = m_RenderCommandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			FROST_VKCHECK(vkAllocateCommandBuffers(device, &allocInfo, &m_RenderCommandBuffer[i]));
		}
	}

	void VulkanSwapChain::CreateComputeCommandBuffer()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Creating the compute comamnd pool
		VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		cmdPoolInfo.queueFamilyIndex = VulkanContext::GetCurrentDevice()->GetQueueFamilies().ComputeFamily.Index;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		FROST_VKCHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &m_ComputeCommandPool));

		m_ComputeCommandBuffer.resize(m_SwapChainImages.size());
		for (uint32_t i = 0; i < Renderer::GetRendererConfig().FramesInFlight; i++)
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = m_ComputeCommandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			FROST_VKCHECK(vkAllocateCommandBuffers(device, &allocInfo, &m_ComputeCommandBuffer[i]));
		}
	}

	void VulkanSwapChain::BeginFrame(VkSemaphore imageAvailableSemaphore, uint32_t* imageIndex)
	{
		*imageIndex = AcquireImage(imageAvailableSemaphore);
	}

	void VulkanSwapChain::BeginRenderPass()
	{

		VkRenderPass vkRenderPass = m_RenderPass;
		VkImageView swapChainImageView = m_SwapChainImageViews[m_CurrentFrameIndex];
		VkExtent2D extent = m_SwapChainExtent;

		VkRenderPassAttachmentBeginInfo attachmentInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
		attachmentInfo.attachmentCount = 1;
		attachmentInfo.pAttachments = &swapChainImageView;

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vkRenderPass;
		renderPassInfo.framebuffer = m_Framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { extent.width, extent.height };
		renderPassInfo.pNext = &attachmentInfo; // Imageless framebuffer

		std::array<VkClearValue, 2> vkClearValues;
		vkClearValues[0].color = { 0.05f, 0.05f, 0.05f, 0.05f };
		vkClearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
		renderPassInfo.pClearValues = vkClearValues.data();


		vkCmdBeginRenderPass(m_RenderCommandBuffer[m_CurrentBufferIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(m_RenderCommandBuffer[m_CurrentBufferIndex], 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = extent;
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(m_RenderCommandBuffer[m_CurrentBufferIndex], 0, 1, &scissor);
	}

	void VulkanSwapChain::Present(VkSemaphore waitSemaphore, uint32_t imageIndex)
	{
		// Start Presenting
		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_SwapChain;
		presentInfo.pImageIndices = &m_CurrentFrameIndex;
		FROST_VKCHECK(vkQueuePresentKHR(m_PresentQueue, &presentInfo));

		RendererConfig rendererConfig = Renderer::GetRendererConfig();
		m_CurrentBufferIndex = (m_CurrentBufferIndex + 1) % rendererConfig.FramesInFlight;
	}

	void VulkanSwapChain::PickPresentQueue()
	{
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// Getting all the queueFamilies
		Vector<VkQueueFamilyProperties> queueFamilyProps;
		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
		queueFamilyProps.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProps.data());

		// Picking the one that has presentQueue
		for (uint32_t i = 0; i < queueFamilyProps.size(); i++)
		{
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &presentSupport);

			if (presentSupport)
			{
				m_PresentQueueIndex = i;
				break;
			}
		}

		// Getting the VkQueue using the presentQueueIndex
		vkGetDeviceQueue(device, m_PresentQueueIndex, 0, &m_PresentQueue);
	}

	void VulkanSwapChain::Destroy()
	{
		VkInstance instance = VulkanContext::GetInstance();
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		for (uint32_t i = 0; i < Renderer::GetRendererConfig().FramesInFlight; i++)
		{
			vkFreeCommandBuffers(device, m_RenderCommandPool, 1, &m_RenderCommandBuffer[i]);
			vkFreeCommandBuffers(device, m_ComputeCommandPool, 1, &m_ComputeCommandBuffer[i]);
		}
		vkDestroyCommandPool(device, m_RenderCommandPool, nullptr);
		vkDestroyCommandPool(device, m_ComputeCommandPool, nullptr);

		vkDestroyRenderPass(device, m_RenderPass, nullptr);
		vkDestroyFramebuffer(device, m_Framebuffer, nullptr);
		for (auto imageView : m_SwapChainImageViews)
			vkDestroyImageView(device, imageView, nullptr);

		vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
		vkDestroySurfaceKHR(instance, m_Surface, nullptr);
	}
	
	uint32_t VulkanSwapChain::AcquireImage(VkSemaphore availableSemaphore)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t imageIndex;
		VkResult result;

		result = vkAcquireNextImageKHR(device, m_SwapChain, UINT64_MAX, availableSemaphore, VK_NULL_HANDLE, &imageIndex);
		m_CurrentFrameIndex = imageIndex;

		switch (result)
		{
			case VK_ERROR_OUT_OF_DATE_KHR:				Resize(0, 0); break;
			case VK_SUCCESS: case VK_SUBOPTIMAL_KHR:	break;
			default:									FROST_ASSERT(false, "Failed to acquire swap chain image!");
		}

		return imageIndex;
	}

}