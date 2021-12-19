#include "frostpch.h"
#include "VulkanContext.h"

#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Renderer/Renderer.h"

#include "VkBootstrap.h"

#include <GLFW/glfw3.h>

namespace Frost
{

#ifdef FROST_DEBUG
	static bool s_EnableValidationLayers = true;
#else
	static bool s_EnableValidationLayers = false;
#endif


	namespace Utils
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

			FROST_VKCHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));


			if (settings.Validation)
			{
				auto createDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

				VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo{};
				debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
				debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
				debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
				debugUtilsMessengerCreateInfo.pfnUserCallback = DebugCallback;

				FROST_VKCHECK(createDebugMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugUtilsMessenger));
			}


		}

	}

	VkInstance VulkanContext::m_Instance;
	uint32_t VulkanContext::m_VulkanAPIVersion = VK_VERSION_1_2;

	VkDebugUtilsMessengerEXT VulkanContext::m_DebugMessenger;

	Scope<VulkanDevice> VulkanContext::m_Device;
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
		m_Device->ShutDown();
	}

	void VulkanContext::Init()
	{

		m_Device = CreateScope<VulkanDevice>();

#if 0
		const uint32_t appVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
		const uint32_t apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);
	
		vkb::InstanceBuilder instance_builder;
		instance_builder.set_app_name("Frost Engine");
		instance_builder.set_app_version(appVersion);
		instance_builder.require_api_version(apiVersion);
#ifdef FROST_DEBUG
		instance_builder.set_debug_callback(Utils::DebugCallback);
		instance_builder.enable_validation_layers(true);
		instance_builder.enable_layer("VK_LAYER_LUNARG_monitor");
#endif


		m_SupportedInstanceExtensions = {
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
			VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
			VK_KHR_SURFACE_EXTENSION_NAME,
			VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
			VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
		};

		auto sysInfo = vkb::SystemInfo::get_system_info().value();


		// Print all available extensions
		FROST_CORE_TRACE("Instance extensions: ");
		for (auto& extension : sysInfo.available_extensions)
		{
			FROST_CORE_TRACE(" || {0}", extension.extensionName);
		}

		// Add all available extensions
		for (auto ext : m_SupportedInstanceExtensions)
		{
			if (!sysInfo.is_extension_available(ext))
			{
				FROST_CORE_CRITICAL("{0} is not available!\n", ext);
				FROST_ASSERT(false, "Assert requested!");
				continue;
			}
			instance_builder.enable_extension(ext);
		}

		// Build the instance
		auto inst_ret = instance_builder.build();
		if (!inst_ret)
		{
			FROST_CORE_CRITICAL("Failed to create Vulkan instance. Error: {0}", inst_ret.error().message());
		}

		vkb::Instance instance = inst_ret.value();
		m_Instance = instance.instance;
		m_DebugMessenger = instance.debug_messenger;
		
		//m_PhysicalDevice = CreateScope<VulkanPhysicalDevice>(instance);
#endif

		m_Device->Init(m_Instance, m_DebugMessenger);
		//m_Device->Init(m_PhysicalDevice);
		m_SwapChain = CreateScope<VulkanSwapChain>(m_Window);
		VulkanAllocator::Init();
		VulkanBindlessAllocator::Init();

	}

	void VulkanContext::Resize(uint32_t width, uint32_t height)
	{
		VkDevice device = m_Device->GetVulkanDevice();
		vkDeviceWaitIdle(device);
		m_SwapChain->Resize(width, height);
		vkDeviceWaitIdle(device);
	}

	void VulkanContext::WaitDevice()
	{
		vkDeviceWaitIdle(m_Device->GetVulkanDevice());
	}

	GPUMemoryStats VulkanContext::GetGPUMemoryStats() const
	{
		return VulkanAllocator::GetMemoryStats();
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