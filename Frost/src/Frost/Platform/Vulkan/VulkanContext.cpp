#include "frostpch.h"
#include "VulkanContext.h"

#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/Internal/VulkanExtensions.h"

//#include "VkBootstrap.h"

#include <GLFW/glfw3.h>

namespace Frost
{

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	static std::vector<const char*> GetRequiredExtensions(bool validationLayers);
	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);


	VkInstance VulkanContext::m_Instance;
	uint32_t VulkanContext::m_VulkanAPIVersion = VK_VERSION_1_2;

	VkDebugUtilsMessengerEXT VulkanContext::m_DebugMessenger;

	bool VulkanContext::m_EnableValidationLayers;

	Scope<VulkanDevice> VulkanContext::m_LogicalDevice;
	Scope<VulkanPhysicalDevice> VulkanContext::m_PhysicalDevice;


	Scope<VulkanSwapChain> VulkanContext::m_SwapChain;


	VulkanContext::VulkanContext(GLFWwindow* window)
		: m_Window(window)
	{

	}

	VulkanContext::~VulkanContext()
	{
		VulkanBindlessAllocator::ShutDown();
		VulkanAllocator::ShutDown();
		m_SwapChain->Destroy();

		// Destroy the logical device
		m_LogicalDevice->ShutDown();

		// Destroy the debug messenger if it is enabled
		if (m_EnableValidationLayers)
		{
			auto destroyDebugMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
			destroyDebugMessenger(m_Instance, m_DebugMessenger, nullptr);
		}
		// Destroy the instance
		vkDestroyInstance(m_Instance, nullptr);
	}

	void VulkanContext::Init()
	{

#ifdef FROST_DEBUG
		m_EnableValidationLayers = true;
#else
		m_EnableValidationLayers = false;
#endif
		CreateInstance();
		CreateDebugMessenger();
		m_PhysicalDevice = CreateScope<VulkanPhysicalDevice>(m_Instance);
		m_LogicalDevice = CreateScope<VulkanDevice>();
		m_LogicalDevice->Init(m_PhysicalDevice);
		LoadVulkanFunctionPointers();

		m_SwapChain = CreateScope<VulkanSwapChain>(m_Window);
		VulkanAllocator::Init();
		VulkanBindlessAllocator::Init();
	}

	void VulkanContext::CreateInstance()
	{
		const std::vector<const char*> validationLayers = {
			"VK_LAYER_KHRONOS_validation"
		};

		VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.pApplicationName = "Frost Engine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 2, 0);
		appInfo.pEngineName = "Frost Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 2, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		createInfo.pApplicationInfo = &appInfo;

		auto instanceExtensions = GetRequiredExtensions(m_EnableValidationLayers);
		createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
		createInfo.ppEnabledExtensionNames = instanceExtensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

		if (m_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		FROST_VKCHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));
	}

	void VulkanContext::CreateDebugMessenger()
	{
		if (!m_EnableValidationLayers)
		{
			return;
		}

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		auto createDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
		FROST_VKCHECK(createDebugMessenger(m_Instance, &createInfo, nullptr, &m_DebugMessenger));
	}

	void VulkanContext::LoadVulkanFunctionPointers()
	{
		load_VK_EXTENSION_SUBSET(m_Instance, vkGetInstanceProcAddr, m_LogicalDevice->GetVulkanDevice(), vkGetDeviceProcAddr);
		//load_VK_EXT_debug_utils(m_Instance, vkGetInstanceProcAddr, m_LogicalDevice->GetVulkanDevice(), vkGetDeviceProcAddr);

		vk::DynamicLoader dl;
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_LogicalDevice->GetVulkanDevice());
	}


	void VulkanContext::Resize(uint32_t width, uint32_t height)
	{
		VkDevice device = m_LogicalDevice->GetVulkanDevice();
		vkDeviceWaitIdle(device);
		m_SwapChain->Resize(width, height);
		vkDeviceWaitIdle(device);
	}

	void VulkanContext::WaitDevice()
	{
		vkDeviceWaitIdle(m_LogicalDevice->GetVulkanDevice());
	}

	GPUMemoryStats VulkanContext::GetGPUMemoryStats() const
	{
		return VulkanAllocator::GetMemoryStats();
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





	// STATIC FUNCTIONS
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			FROST_CORE_WARN("[PERFORMANCE_WARNING] Validation layer : {0}", pCallbackData->pMessage);

		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			FROST_CORE_WARN("[WARNING] Validation layer : {0}", pCallbackData->pMessage);
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			FROST_CORE_ERROR("[ERROR] Validation layer : {0}", pCallbackData->pMessage);
		else if (!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT))
			FROST_CORE_INFO("[IFNO] Validation layer : {0}", pCallbackData->pMessage);

		return VK_FALSE;
	}


	static std::vector<const char*> GetRequiredExtensions(bool validationLayers)
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (validationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
		extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		return extensions;
	}

	static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = &DebugCallback;
	}
}