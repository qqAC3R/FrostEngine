#include "frostpch.h"
#include "VulkanSwapChain.h"

#include "Frost/Core/Application.h"
#include "Platform/Vulkan/VulkanContext.h"

#include "Frost/Core/Engine.h"

namespace Frost
{

	namespace VulkanUtils
	{
		struct SwapChainDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		static SwapChainDetails QuerySwapChainSupport(const VkPhysicalDevice& device)
		{
			SwapChainDetails details;
			VkSurfaceKHR surface = VulkanContext::GetSurface();

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);


			// Get swapchain formats
			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

			if (formatCount != 0)
			{
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
			}


			// Get swapchain presentModes
			uint32_t presentCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);

			if (presentCount != 0)
			{
				details.presentModes.resize(presentCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes.data());
			}


			return details;
		}

		static VkSurfaceFormatKHR GetSwapChainSurfaceFormat()
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
			auto availableFormats = QuerySwapChainSupport(physicalDevice).formats;

			for (const auto& availableFormat : availableFormats)
			{
				if (availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					return availableFormat;
				}
			}

			return availableFormats[0];
		}

		static VkPresentModeKHR GetSwapChainPresentMode()
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
			auto availablePresentModes = QuerySwapChainSupport(physicalDevice).presentModes;

			for (const auto& availablePresentMode : availablePresentModes)
			{
				if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					return availablePresentMode;
				}

			}

			return VK_PRESENT_MODE_FIFO_KHR;
		}


		static VkExtent2D GetSwapChainExtent()
		{
			VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
			auto capabilities = QuerySwapChainSupport(physicalDevice).capabilities;

			if (capabilities.currentExtent.width != UINT32_MAX)
			{
				return capabilities.currentExtent;
			}
			else
			{
				uint32_t width = Application::Get().GetWindow().GetWidth();
				uint32_t height = Application::Get().GetWindow().GetHeight();

				VkExtent2D actualExtent = {
					static_cast<uint32_t>(width),
					static_cast<uint32_t>(height)
				};

				actualExtent.width = std::max<uint32_t>(capabilities.minImageExtent.width,
									 std::min<uint32_t>(capabilities.maxImageExtent.width, actualExtent.width));

				actualExtent.height = std::max<uint32_t>(capabilities.maxImageExtent.height,
									  std::min<uint32_t>(capabilities.maxImageExtent.height, actualExtent.height));

				return actualExtent;
			}
		}
	}
	

	VulkanSwapChain::VulkanSwapChain()
	{
		CreateSwapChain();
	}
	
	VulkanSwapChain::~VulkanSwapChain()
	{
	}

	void VulkanSwapChain::CreateSwapChain()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkPhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();
		VkSurfaceKHR surface = VulkanContext::GetSurface();


		VulkanUtils::SwapChainDetails swapChainSupport = VulkanUtils::QuerySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = VulkanUtils::GetSwapChainSurfaceFormat();
		VkPresentModeKHR presentMode = VulkanUtils::GetSwapChainPresentMode();
		VkExtent2D extent = VulkanUtils::GetSwapChainExtent();


		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

		if (swapChainSupport.capabilities.minImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}


		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


		//QueueFamilyIndices indices = VulkanContext::GetCurrentDevice()->FindQueueFamilies(physicalDevice);
		//uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };



		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = nullptr;

#if 0   // TODO: Maybe add this later, for now I want the most performance

		// If present queue and graphics queue are not from the family, then the swap chainimages can be used across all families without explicit ownership transfers
		/*
			VK_SHARING_MODE_EXCLUSIVE:  An image is owned by one queue family at a time and ownership must be explicitly transferred
										before using it in another queue family. This option offers the best performance.

			VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue families without explicit ownership transfers.
		*/
		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}
#endif

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			FROST_ASSERT(0, "failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, m_SwapChain, &imageCount, m_SwapChainImages.data());



		VulkanContext::SetStructDebugName("SwapChain", VK_OBJECT_TYPE_SWAPCHAIN_KHR, m_SwapChain);

	}

	VkExtent2D VulkanSwapChain::GetExtent()
	{
		return VulkanUtils::GetSwapChainExtent();
	}

	VkFormat VulkanSwapChain::GetImageFormat()
	{
		return VulkanUtils::GetSwapChainSurfaceFormat().format;
	}

	void VulkanSwapChain::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroySwapchainKHR(device, m_SwapChain, nullptr);
	}
	
}