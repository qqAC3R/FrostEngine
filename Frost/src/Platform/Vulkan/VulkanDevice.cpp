#include "frostpch.h"
#include "VulkanDevice.h"
#include "Frost/Core/Engine.h"

//#include "Frost/Core/StaticFunctions.h"
#include "VulkanContext.h"

namespace Frost
{

	VulkanDevice::VulkanDevice()
	{

	}


	VulkanDevice::~VulkanDevice()
	{
		//vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		//vkctx.deinit();

	}

	void VulkanDevice::ShutDown()
	{
		vkDeviceWaitIdle(m_Device);
		//vkDestroyDevice(m_Device, nullptr);
	}

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


	namespace VulkanUtils
	{
		int GetQueueFamilyIndex(VkPhysicalDevice physicalDevice, VkQueueFlagBits queueFlags)
		{
			Vector<VkQueueFamilyProperties> queueFamilyProperties;

			uint32_t queueFamilyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			assert(queueFamilyCount > 0);
			queueFamilyProperties.resize(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());


			// Dedicated queue for compute
			// Try to find a queue family index that supports compute but not graphics
			if (queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
				{
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
					{
						return i;
					}
				}
			}

			// Dedicated queue for transfer
			// Try to find a queue family index that supports transfer but not graphics and compute
			if (queueFlags & VK_QUEUE_TRANSFER_BIT)
			{
				for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
				{
					if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
					{
						return i;
					}
				}
			}

			// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
			for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
			{
				if (queueFamilyProperties[i].queueFlags & queueFlags)
				{
					return i;
				}
			}

			//FROST_ASSERT(false, "Family queue not found!");
			return -1;

		}

		static uint32_t RatePhysicalDevice(const VkPhysicalDevice& device)
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(device, &deviceProperties);
			VkPhysicalDeviceFeatures deviceFeatures;
			vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

			//std::cout << deviceProperties.deviceName << std::endl;

			//FROST_CORE_TRACE("Device:");
			//FROST_CORE_TRACE("    API: Vulkan");
			//FROST_CORE_TRACE("    Name: {0}", deviceProperties.deviceName);

			uint32_t score = 0;

			if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				score += 1000;

			score += deviceProperties.limits.maxImageDimension2D;

			if (!deviceFeatures.geometryShader)
				return 0;

			return score;
		}


		bool IsDeviceSuitable(VkPhysicalDevice physicalDevice)
		{
			bool graphicsQueue = GetQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT) == -1 ? false : true;
			bool computeQueue = GetQueueFamilyIndex(physicalDevice, VK_QUEUE_COMPUTE_BIT) == -1 ? false : true;
			bool transferQueue = GetQueueFamilyIndex(physicalDevice, VK_QUEUE_TRANSFER_BIT) == -1 ? false : true;


			bool swapChainAdequate = false;
			{
				VkSurfaceKHR surface = VulkanContext::GetSurface();
				Vector<VkSurfaceFormatKHR> formats;
				Vector<VkPresentModeKHR> presentModes;


				// Get swapchain formats
				uint32_t formatCount = 0;
				vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
				if (formatCount != 0)
				{
					formats.resize(formatCount);
					vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
				}


				// Get swapchain presentModes
				uint32_t presentCount = 0;
				vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
				if (presentCount != 0)
				{
					presentModes.resize(presentCount);
					vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes.data());
				}

				swapChainAdequate = !formats.empty() && !presentModes.empty();
			}

			VkPhysicalDeviceFeatures supportedFeatures{};
			vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
			bool samplerAnisotropy = supportedFeatures.samplerAnisotropy;


			return graphicsQueue && computeQueue && transferQueue && swapChainAdequate && samplerAnisotropy;

		}


		static VkPhysicalDevice SelectPhysicalDevice(VkInstance& instance)
		{
			uint32_t deviceCount = 0;
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

			if (deviceCount == 0)
			{
				FROST_ASSERT(0, "Failed to find a GPU with Vulkan Support!");
			}

			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());



			std::multimap<int, VkPhysicalDevice> candidates;
			for (const auto& device : devices)
			{
				// Rating the device
				int score = RatePhysicalDevice(device);

				// If the device has the application's requirments then add a point
				if (IsDeviceSuitable(device))
					score += 1;

				candidates.insert(std::make_pair(score, device));
			}

			if (candidates.rbegin()->first > 0)
				return candidates.rbegin()->second;


			FROST_ASSERT(0, "Failed to find a suitable GPU!");
			return VkPhysicalDevice();

		}

	}


#if 0
	void VulkanDevice::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			FROST_ASSERT(0, "Failed to find a GPU with Vulkan Support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(*m_Instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device))
			{
				m_PhysicalDevice = device;
				break;
			}
		}

		std::multimap<int, VkPhysicalDevice> candidates;

		for (const auto& device : devices)
		{
			int score = RateDevice(device);
			candidates.insert(std::make_pair(score, device));
		}

		if (candidates.rbegin()->first > 0)
		{
			m_PhysicalDevice = candidates.rbegin()->second;

		}
		else
		{
			FROST_ASSERT(0, "Failed to find a suitable GPU!");
		}
	}



	bool VulkanDevice::IsDeviceSuitable(const VkPhysicalDevice& device)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device);

		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainDetails swapChainSupport = QuerySwapChainSupport(device, *m_Surface);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures{};
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);


		return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;

	}

	bool VulkanDevice::CheckDeviceExtensionSupport(const VkPhysicalDevice& device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(m_DeviceExtension.begin(), m_DeviceExtension.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();


	}
#endif



	VkFormat VulkanDevice::FindDepthFormat()
	{
		return FindSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	VkFormat VulkanDevice::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		FROST_ASSERT(0, "Failed to find support format!");

		return VK_FORMAT_UNDEFINED;

	}


#if 1
	QueueFamilyIndices VulkanDevice::FindQueueFamilies(const VkPhysicalDevice& device)
	{
		VkSurfaceKHR surface = VulkanContext::GetSurface();

		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			//if (queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT))
			if (queueFamily.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT))
			{
				indices.graphicsFamily = i;
			}
			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
			{
				indices.computeFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete()) {
				break;
			}

			i++;
		}


		return indices;


	}
#endif




#if 0
	void VulkanDevice::CreateLogicalDevice(const std::vector<const char*>& validationLayers)
	{
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.0f;

		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		//VkPhysicalDeviceFeatures deviceFeatures{};

		//VkDeviceCreateInfo createInfo{};
		//createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		//
		//createInfo.pQueueCreateInfos = queueCreateInfos.data();
		//createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		//
		//createInfo.enabledExtensionCount = static_cast<uint32_t>(m_Extensions.size());
		//createInfo.ppEnabledExtensionNames = m_Extensions.data();
		//createInfo.pEnabledFeatures = &deviceFeatures;



		//QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

		//std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		//std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		//
		//float queuePriority = 1.0f;
		//
		//for (uint32_t queueFamily : uniqueQueueFamilies)
		//{
		//	VkDeviceQueueCreateInfo queueCreateInfo{};
		//	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		//	queueCreateInfo.queueFamilyIndex = queueFamily;
		//	queueCreateInfo.queueCount = 1;
		//	queueCreateInfo.pQueuePriorities = &queuePriority;
		//	queueCreateInfos.push_back(queueCreateInfo);
		//}

		std::vector<const char*> activatedDeviceExtensions;
		activatedDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		//
		activatedDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		activatedDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		activatedDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		activatedDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		activatedDeviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
		activatedDeviceExtensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

		activatedDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		activatedDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);


		//activatedDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		//activatedDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		////activatedDeviceExtensions.push_back(VK_NV_RAY_TRACING_EXTENSION_NAME);
		//activatedDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		//activatedDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		//activatedDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		//activatedDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		//activatedDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);


		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
		accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR raytracingPipelineFeatures = {};
		raytracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
		raytracingPipelineFeatures.pNext = &accelerationStructureFeatures;

		VkPhysicalDeviceVulkan12Features vulkan12Features = {};
		vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		vulkan12Features.pNext = &raytracingPipelineFeatures;

		VkPhysicalDeviceFeatures2 features2 = {};
		features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features2.pNext = &vulkan12Features;

		vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &features2);

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.pNext = &features2;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.queueCreateInfoCount = 1;
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(activatedDeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = activatedDeviceExtensions.data();



		std::vector<void*> features;
		features.push_back((void*)&accelFeature);
		features.push_back((void*)&rtPipelineFeature);

		//createInfo.pNext = features.data();



		if (!validationLayers.empty())
		{
			deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			deviceCreateInfo.enabledLayerCount = 0;
		}


		VkResult result = vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device);
		if (result != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to create logical device!");
		}

		FROST_CORE_TRACE("Created physical device!");





		//for (auto& queue : vulkanQueue)
		//{
		//	vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, *(&queue));
		//
		//}

		vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_Queues.m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_Queues.m_PresentQueue);



	}

#endif
	//void VulkanDevice::InitVkHpp(VkInstance& instance)
	//{
	//	//vk::DynamicLoader dl;
	//	//PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
	//	//VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
	//	//VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
	//	//VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);
	//
	//}



#if 0
	void VulkanDevice::CreateInstance(VkApplicationInfo& applicationInfo)
	{


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
#elif defined(FROST_DEBUG)
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif



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
					m_SupportedInstanceExtensions.push_back(extension.extensionName);
				}
			}
		}


		VkInstanceCreateInfo instanceCreateInfo = {};
		instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instanceCreateInfo.pNext = NULL;
		instanceCreateInfo.pApplicationInfo = &applicationInfo;
		if (instanceExtensions.size() > 0)
		{
			if (m_Settings.Validation)
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
		if (m_Settings.Validation)
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

		FROST_VKCHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_Instance), "");


		if (m_Settings.Validation)
		{
			VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
			debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;
			VkResult result = vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsMessengerCI, nullptr, &debugUtilsMessenger);
			assert(result == VK_SUCCESS);
		}


	}

#endif

	void VulkanDevice::CreateDevice(std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
	{



		// Desired queues need to be requested upon logical device creation
		// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
		// requests different queue types

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

		// Get queue family indices for the requested queue family types
		// Note that the indices may overlap depending on the implementation
		const float defaultQueuePriority(0.0f);

		// Graphics queue
		if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
		{
			uint32_t graphicsFamilyIndex = VulkanUtils::GetQueueFamilyIndex(m_PhysicalDevice, VK_QUEUE_GRAPHICS_BIT);

			m_QueueFamilies.GraphicsFamily.Index = graphicsFamilyIndex;

			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = graphicsFamilyIndex;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
		else
		{
			m_QueueFamilies.GraphicsFamily.Index = 0;
		}

		// Dedicated compute queue
		if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
		{
			uint32_t computeFamilyIndex = VulkanUtils::GetQueueFamilyIndex(m_PhysicalDevice, VK_QUEUE_COMPUTE_BIT);

			m_QueueFamilies.ComputeFamily.Index = computeFamilyIndex;


			if (m_QueueFamilies.ComputeFamily.Index != m_QueueFamilies.GraphicsFamily.Index)
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = m_QueueFamilies.ComputeFamily.Index;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			// Else we use the same queue
			m_QueueFamilies.ComputeFamily.Index = m_QueueFamilies.GraphicsFamily.Index;
		}

		// Dedicated transfer queue
		if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
		{
			uint32_t transferFamilyIndex = VulkanUtils::GetQueueFamilyIndex(m_PhysicalDevice, VK_QUEUE_TRANSFER_BIT);
			uint32_t graphicsFamilyIndex = m_QueueFamilies.GraphicsFamily.Index;
			uint32_t computeFamilyIndex = m_QueueFamilies.ComputeFamily.Index;

			m_QueueFamilies.TransferFamily.Index = transferFamilyIndex;


			if ((transferFamilyIndex != graphicsFamilyIndex) && (transferFamilyIndex != computeFamilyIndex))
			{
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queueInfo{};
				queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queueInfo.queueFamilyIndex = transferFamilyIndex;
				queueInfo.queueCount = 1;
				queueInfo.pQueuePriorities = &defaultQueuePriority;
				queueCreateInfos.push_back(queueInfo);
			}
		}
		else
		{
			// Else we use the same queue
			m_QueueFamilies.TransferFamily.Index = m_QueueFamilies.GraphicsFamily.Index;
		}

		// Create the logical device representation
		std::vector<const char*> deviceExtensions(enabledExtensions);
		if (useSwapChain)
		{
			// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
			deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		}

		VkPhysicalDeviceFeatures physicalDeviceFeatures{};

		VkDeviceCreateInfo deviceCreateInfo = {};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
		deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;

		// If a pNext(Chain) has been passed, we need to add it to the device creation info
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		if (pNextChain)
		{
			physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			physicalDeviceFeatures2.features = physicalDeviceFeatures;
			physicalDeviceFeatures2.pNext = pNextChain;
			deviceCreateInfo.pEnabledFeatures = nullptr;
			deviceCreateInfo.pNext = &physicalDeviceFeatures2;
		}

		// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
		if (ExtensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
		{
			deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			//enableDebugMarkers = true;
		}

		if (deviceExtensions.size() > 0)
		{
			deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
			deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
		}

		FROST_VKCHECK(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device), "Failed to create the physical/logical device!");




		{
			// Getting the present family queue (which we get from the VkSurfaceKHR)

			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

			std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
			VkSurfaceKHR surface = VulkanContext::GetSurface();


			uint32_t i = 0;
			for (const auto& queueFamily : queueFamilies)
			{

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, surface, &presentSupport);

				if (presentSupport)
				{
					m_QueueFamilies.PresentFamily.Index = i;
				}
				i++;
			}
		}


		//m_EnabledFeatures = enabledFeatures;
	}


	static uint32_t GetPresentQueue(VkPhysicalDevice device)
	{
		VkSurfaceKHR surface = VulkanContext::GetSurface();

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());


		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				return i;
			}

			i++;
		}
	}


	void VulkanDevice::Init(VkInstance& instance, VkDebugUtilsMessengerEXT& dbMessenger)
	{

#if 0
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Frost Engine";
		appInfo.pEngineName = "Frost";
		appInfo.apiVersion = m_VulkanApiVersion;


#ifdef FROST_DEBUG
		m_Settings.Validation = true;
#endif

		Vector<const char*>

			CreateInstance(appInfo);
#endif




#if 0
		{
			// Enable features required for ray tracing using feature chaining via pNext		
			m_BufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			m_BufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;

			m_RayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
			m_RayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
			m_RayTracingPipelineFeatures.pNext = &m_BufferDeviceAddresFeatures;

			m_AccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
			m_AccelerationStructureFeatures.accelerationStructure = VK_TRUE;
			m_AccelerationStructureFeatures.pNext = &m_RayTracingPipelineFeatures;

			m_pNextChain = &m_AccelerationStructureFeatures;
		}


		// Ray Tracing related - RTPipeline/AccelerationStructures
		m_EnabledDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

		// Getting the device address from vkBuffers
		m_EnabledDeviceExtensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

		// Scalar buffers in shaders
		m_EnabledDeviceExtensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

		// Random numbers in shaders (used by the path tracer)
		m_EnabledDeviceExtensions.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);


		// I dont what they do, but they are required for the extenions above :kekw:
		m_EnabledDeviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		m_EnabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);



		m_QueueFamilies.GraphicsFamily = new QueueFamilies::QueueFamily();
		m_QueueFamilies.ComputeFamily = new QueueFamilies::QueueFamily();
		m_QueueFamilies.TransferFamily = new QueueFamilies::QueueFamily();
		m_QueueFamilies.PresentFamily = new QueueFamilies::QueueFamily();

		//m_PhysicalDevice = VulkanUtils::SelectPhysicalDevice(instance);
		//CreateDevice(m_EnabledDeviceExtensions, m_pNextChain, true, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);


		nvvk::ContextCreateInfo contextInfo;
		contextInfo.setVersion(1, 2);
		contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
		contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
		contextInfo.addInstanceExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, true);
		contextInfo.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
		contextInfo.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

		//#RayTracing_Extensions
		contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);
		contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);
		contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);

		auto compatibleDevices = context.getCompatibleDevices(contextInfo);
		assert(!compatibleDevices.empty());
		context.initDevice(compatibleDevices[0], contextInfo);

		m_PhysicalDevice = context.getPhysicalDevice();
		m_Device = context.getDevice();

		auto familyIndicies = FindQueueFamilies(m_PhysicalDevice);

		m_QueueFamilies.GraphicsFamily->Index = familyIndicies.graphicsFamily.value();;
		m_QueueFamilies.ComputeFamily->Index = familyIndicies.computeFamily.value();
		//m_QueueFamilies.TransferFamily->Index = context.getQueueFamily(VK_QUEUE_TRANSFER_BIT);

		m_QueueFamilies.PresentFamily->Index = familyIndicies.presentFamily.value();

		//context.getQueueFamily({}, {}, VulkanContext::GetSurface());









		uint32_t queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
		FROST_ASSERT(queueFamilyCount > 0, "There are no queue families!");
		m_QueueFamilyProperties.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, m_QueueFamilyProperties.data());

		vkGetDeviceQueue(m_Device, m_QueueFamilies.GraphicsFamily->Index, 0, &m_QueueFamilies.GraphicsFamily->Queue);
		vkGetDeviceQueue(m_Device, m_QueueFamilies.PresentFamily->Index, 0, &m_QueueFamilies.PresentFamily->Queue);
		vkGetDeviceQueue(m_Device, m_QueueFamilies.ComputeFamily->Index, 0, &m_QueueFamilies.ComputeFamily->Queue);
		vkGetDeviceQueue(m_Device, m_QueueFamilies.TransferFamily->Index, 0, &m_QueueFamilies.TransferFamily->Queue);
#endif







		nvvk::ContextCreateInfo contextInfo;
		contextInfo.setVersion(1, 2);
		contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
		contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true);
		contextInfo.addInstanceExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, true);
		contextInfo.addInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME);
		contextInfo.addInstanceExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		contextInfo.addInstanceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

		contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);

		//#RayTracing_Extensions
		contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false, &accelFeature);
		contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false, &rtPipelineFeature);
		contextInfo.addDeviceExtension(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
		contextInfo.addDeviceExtension(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);




		vkctx.initInstance(contextInfo, "Frost Engine", DebugCallback);

		auto compatibleDevices = vkctx.getCompatibleDevices(contextInfo);
		assert(!compatibleDevices.empty());

		vkctx.initDevice(compatibleDevices[0], contextInfo);



		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(vkctx.getPhysicalDevice(), &deviceProperties);

		FROST_CORE_INFO("Renderer:");
		FROST_CORE_INFO("  Vulkan Version: {0}.{1}", contextInfo.apiMajor, contextInfo.apiMinor);
		FROST_CORE_INFO("  Device: {0}", deviceProperties.deviceName);



		m_PhysicalDevice = vkctx.getPhysicalDevice();
		m_Device = vkctx.getDevice();

		instance = vkctx.getInstance();
		dbMessenger = vkctx.getDbMessenger();


		vk::DynamicLoader dl;
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);

	}



	void VulkanDevice::SetQueues(VkSurfaceKHR surface)
	{

		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);



		m_QueueFamilies.GraphicsFamily.Index = indices.graphicsFamily.value();
		m_QueueFamilies.ComputeFamily.Index = indices.computeFamily.value();
		m_QueueFamilies.PresentFamily.Index = indices.presentFamily.value();

		vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_QueueFamilies.GraphicsFamily.Queue);
		vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_QueueFamilies.PresentFamily.Queue);
		vkGetDeviceQueue(m_Device, indices.computeFamily.value(), 0, &m_QueueFamilies.ComputeFamily.Queue);
	}

}
