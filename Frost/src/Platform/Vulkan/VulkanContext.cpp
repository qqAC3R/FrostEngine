#include "frostpch.h"
#include "VulkanContext.h"

#include "Platform/Vulkan/Buffers/VulkanBufferAllocator.h"
#include "Platform/Vulkan/VulkanSyncObjects.h"

#include <GLFW/glfw3.h>
//#include "nvvk/context_vk.hpp"

namespace Frost
{

#ifdef FROST_DEBUG
	static bool s_EnableValidationLayers = true;
#else
	static bool s_EnableValidationLayers = false;
#endif


	namespace VulkanUtils
	{

		static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData)
		{

			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
				FROST_CORE_WARN("[WARNING] Validation layer : {0}", pCallbackData->pMessage);
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				FROST_CORE_ERROR("[ERROR] Validation layer : {0}", pCallbackData->pMessage);
			else if (!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT))
				FROST_CORE_INFO("[IFNO] Validation layer : {0}", pCallbackData->pMessage);

			return VK_FALSE;
		}


#if 0
		static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
			const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			const VkAllocationCallbacks* pAllocator,
			VkDebugUtilsMessengerEXT* pDebugMessenger)
		{

			auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
			if (func != nullptr)
				return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
			else
				return VK_ERROR_EXTENSION_NOT_PRESENT;


			VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
		}
#endif

		static void DestroyDebugUtilsMessengerEXT(VkInstance instance,
			VkDebugUtilsMessengerEXT debugMessenger,
			const VkAllocationCallbacks* pAllocator)
		{
			auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			if (func != nullptr) {
				func(instance, debugMessenger, pAllocator);
			}
		}

#if 0
		static std::vector<const char*> GetRequiredExtensions() {
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

			if (s_EnableValidationLayers)
				extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

			return extensions;
		}

		static bool CheckValidationLayerSupport()
		{
			uint32_t layerCount;
			vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

			std::vector<VkLayerProperties> availableLayers(layerCount);
			vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

			std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
			for (const char* layerName : validationLayers) {
				bool layerFound = false;

				for (const auto& layerProperties : availableLayers) {
					if (strcmp(layerName, layerProperties.layerName) == 0) {
						layerFound = true;
						break;
					}
				}

				if (!layerFound) {
					return false;
				}
			}

			return true;

		}
#endif

		static void CreateSurface(VkInstance& instance, GLFWwindow* window, VkSurfaceKHR& surface)
		{
			if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
				FROST_ASSERT(0, "Failed to create window surface!");

		}

		static void InitVulkanHpp(VkInstance& instance, VkDevice device)
		{
			vk::DynamicLoader dl;
			PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
			VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
			VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
		}


#if 0
		static void CreateInstance(VkInstance& instance, VkDebugUtilsMessengerEXT& debugMessenger)
		{
			if (s_EnableValidationLayers && !CheckValidationLayerSupport())
				FROST_CORE_WARN("Validation layers requested, but not available!");

			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "Hello Triangle";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_2;

			VkInstanceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;

			auto extension = GetRequiredExtensions();
			std::vector<const char*> enabledExtensions;
			enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
			enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
			enabledExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
			enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			enabledExtensions.push_back("VK_LAYER_KHRONOS_validation");


			createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
			createInfo.ppEnabledExtensionNames = enabledExtensions.data();

			// Setup the debugger
			// -------------------------------
			if (s_EnableValidationLayers)
			{
				VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
				PopulateDebugMessengerCreateInfo(debugCreateInfo);

				if (CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &debugMessenger))
					FROST_ASSERT(false, "");


				createInfo.enabledLayerCount = static_cast<uint32_t>(enabledExtensions.size());
				createInfo.ppEnabledLayerNames = enabledExtensions.data();
				createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;


			}
			else
			{
				createInfo.enabledLayerCount = 0;
				createInfo.pNext = nullptr;
			}


			if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
			{
				FROST_ASSERT(0, "Failed to create instance!");
			}

		}
#endif


		static void CreateInstance(VkApplicationInfo& applicationInfo,
			VulkanContext::Settings& settings,
			Vector<const char*>& supportedInstanceExtensions,
			VkInstance& instance,
			VkDebugUtilsMessengerEXT& debugUtilsMessenger)
		{


#if 0
			std::vector<const char*> instanceExtensions = {
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
			};




			// Enable surface extensions depending on os
#if defined(_WIN32)
			instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
			instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
			instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_DIRECTFB_EXT)
			instanceExtensions.push_back(VK_EXT_DIRECTFB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
			instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
			instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
			instanceExtensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
			instanceExtensions.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_HEADLESS_EXT)
			instanceExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
#endif

#endif
			std::vector<const char*> instanceExtensions = {
				VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
				VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
				VK_KHR_SURFACE_EXTENSION_NAME,
				VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
				VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };



			// Get extensions supported by the instance and store for later use
			uint32_t extCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
			if (extCount > 0)
			{
				std::vector<VkExtensionProperties> extensions(extCount);
				if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
				{
					for (const VkExtensionProperties& extension : extensions)
					{
						supportedInstanceExtensions.push_back(extension.extensionName);
					}
				}
			}


			VkInstanceCreateInfo instanceCreateInfo = {};
			instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			instanceCreateInfo.pNext = NULL;
			instanceCreateInfo.pApplicationInfo = &applicationInfo;
			if (instanceExtensions.size() > 0)
			{
				if (settings.Validation)
				{
					// For validation layers
					instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

					// For setting up names for vk structs
					instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
				}
				instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
				instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
			}

			// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
			// Note that on Android this layer requires at least NDK r20
			const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
			if (settings.Validation)
			{
				// Check if this layer is available at instance level
				uint32_t instanceLayerCount;
				vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
				std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
				vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
				bool validationLayerPresent = false;
				for (VkLayerProperties layer : instanceLayerProperties)
				{
					if (strcmp(layer.layerName, validationLayerName) == 0)
					{
						validationLayerPresent = true;
						break;
					}
				}
				if (validationLayerPresent)
				{
					instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
					instanceCreateInfo.enabledLayerCount = 1;
				}
				else
				{
					FROST_CORE_INFO("Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled");
				}
			}

			FROST_VKCHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance), "");


			if (settings.Validation)
			{
				auto createDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

				VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
				debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
				debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
				debugUtilsMessengerCreateInfo.pfnUserCallback = DebugCallback;

				FROST_VKCHECK(createDebugMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugUtilsMessenger), "Failed to create debug messanger!");
			}


		}

	}

	VkInstance VulkanContext::m_Instance;
	VkSurfaceKHR VulkanContext::m_Surface;
	uint32_t VulkanContext::m_VulkanAPIVersion = VK_VERSION_1_2;

	VkDebugUtilsMessengerEXT VulkanContext::m_DebugMessenger;

	Scope<VulkanDevice> VulkanContext::m_Device;
	Scope<VulkanSwapChain> VulkanContext::m_SwapChain;
	Ref<RenderPass> VulkanContext::m_RenderPass;




	VulkanContext::VulkanContext(GLFWwindow* window)
		: m_Window(window)
	{

	}

	VulkanContext::~VulkanContext()
	{
		VulkanBufferAllocator::ShutDown();
		m_RenderPass->Destroy();
		m_SwapChain->Destroy();
		vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);

#if 0
		if (s_EnableValidationLayers)
			VulkanUtils::DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif

		m_Device->ShutDown();

	}

	void VulkanContext::Init()
	{

		m_Device = CreateScope<VulkanDevice>();

		m_Device->Init(m_Instance, m_DebugMessenger);
		VulkanUtils::CreateSurface(m_Instance, m_Window, m_Surface);

		m_Device->SetQueues(m_Surface);
		VulkanBufferAllocator::Init();


		m_SwapChain = CreateScope<VulkanSwapChain>();


		VkExtent2D extent = m_SwapChain->GetExtent();
		FramebufferSpecification framebufferSpec{};
		framebufferSpec.Width = extent.width;
		framebufferSpec.Height = extent.height;
		framebufferSpec.Attachments = { FramebufferTextureFormat::SWAPCHAIN};
		m_RenderPass = RenderPass::Create(framebufferSpec);


		m_SwapChain->CreateFramebuffer(m_RenderPass);
	}

	void VulkanContext::Resize(uint32_t width, uint32_t height)
	{
		ResetSwapChain();
	}


	// This is mainly useful to recreate the swapchain when the window is resized
	void VulkanContext::ResetSwapChain()
	{
		VkDevice device = m_Device->GetVulkanDevice();
		vkDeviceWaitIdle(device);

		m_SwapChain->Destroy();
		m_SwapChain->CreateSwapChain();
		m_SwapChain->CreateFramebuffer(m_RenderPass);

	}

	std::tuple<VkResult, uint32_t> VulkanContext::AcquireNextSwapChainImage(VulkanSemaphore availableSemaphore)
	{
		VkDevice device = m_Device->GetVulkanDevice();
		uint32_t imageIndex;
		VkResult result;

		result = vkAcquireNextImageKHR(device, m_SwapChain->GetVulkanSwapChain(), UINT64_MAX, availableSemaphore.GetSemaphore(), VK_NULL_HANDLE, &imageIndex);

		switch (result)
		{
			case VK_ERROR_OUT_OF_DATE_KHR:				ResetSwapChain(); break;
			case VK_SUCCESS: case VK_SUBOPTIMAL_KHR:	break;
			default:									FROST_ASSERT(false, "Failed to acquire swap chain image!");
		}

		return std::make_tuple(result, imageIndex);
	}


	void VulkanContext::BeginFrame(VkCommandBuffer cmdBuf, uint32_t imageIndex)
	{
		VkRenderPass vkRenderPass = m_RenderPass->GetVulkanRenderPass();
		VkImageView swapChainImageView = m_SwapChain->GetFramebufferAttachment(imageIndex)->GetVulkanImageView();
		VkExtent2D extent = m_SwapChain->GetExtent();

		VkRenderPassAttachmentBeginInfo attachmentInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO };
		attachmentInfo.attachmentCount = 1;
		attachmentInfo.pAttachments = &swapChainImageView;

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vkRenderPass;
		renderPassInfo.framebuffer = m_SwapChain->GetVulkanFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { extent.width, extent.height };
		renderPassInfo.pNext = &attachmentInfo; // Imageless framebuffer


		std::array<VkClearValue, 2> vkClearValues;
		vkClearValues[0].color = { 0.05f, 0.05f, 0.05f, 0.05f };
		vkClearValues[1].depthStencil = { 1.0f, 0 };


		renderPassInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
		renderPassInfo.pClearValues = vkClearValues.data();


		vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


		VkViewport viewport{};
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = extent;
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);



	}

	void VulkanContext::EndFrame(VkCommandBuffer cmdbuf)
	{
		vkCmdEndRenderPass(cmdbuf);
	}

	void VulkanContext::Present(VkSemaphore waitSemaphore, uint32_t imageIndex)
	{
		VkQueue presentQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().PresentFamily.Queue;

		// Start Presenting
		auto swapChain = VulkanContext::GetSwapChain()->GetVulkanSwapChain();
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &waitSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &imageIndex;

		auto result = vkQueuePresentKHR(presentQueue, &presentInfo);

		FROST_VKCHECK(result, "Failed to present swapchain image!");


#if 0
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			// TODO: window resizing
		}
		else if (result != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to present swap chain image!");
		}
#endif
	}

	void VulkanContext::WaitDevice()
	{
		vkDeviceWaitIdle(m_Device->GetVulkanDevice());
	}

	void VulkanContext::InitFunctionPointers()
	{


		//vk::DynamicLoader dl;
		//PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		//PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr = dl.getProcAddress<PFN_vkGetDeviceProcAddr>("vkGetInstanceProcAddr");

		//load_VK_EXTENSION_SUBSET(m_Instance, vkGetInstanceProcAddr, m_Device->GetVulkanDevice(), vkGetDeviceProcAddr);


	}

	VkPipelineBindPoint VulkanContext::GetVulkanGraphicsType(GraphicsType type)
	{
		switch (type)
		{
			case GraphicsType::Graphics:    return VK_PIPELINE_BIND_POINT_GRAPHICS;
			case GraphicsType::Compute:     return VK_PIPELINE_BIND_POINT_COMPUTE;
			case GraphicsType::Raytracing:  return VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
		}

		FROST_ASSERT(false, "GraphicsType not supported!");
		return VkPipelineBindPoint();
	}

}