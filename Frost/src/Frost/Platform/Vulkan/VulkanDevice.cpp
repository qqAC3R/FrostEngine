#include "frostpch.h"
#include "VulkanDevice.h"
#include "Frost/Core/Engine.h"

#include "VulkanContext.h"
#include "VkBootstrap.h"

namespace Frost
{

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if(messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
			FROST_CORE_WARN("[PERFORMANCE_WARNING] Validation layer : {0}", pCallbackData->pMessage);

		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			FROST_CORE_WARN("[WARNING] Validation layer : {0}", pCallbackData->pMessage);
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			FROST_CORE_ERROR("[ERROR] Validation layer : {0}", pCallbackData->pMessage);
		else if (!(messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT))
			FROST_CORE_INFO("[IFNO] Validation layer : {0}", pCallbackData->pMessage);

		return VK_FALSE;
	}

	VulkanDevice::VulkanDevice()
	{
	}

	VulkanDevice::~VulkanDevice()
	{
		//vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
		
	}

	void VulkanDevice::ShutDown()
	{
		vkDeviceWaitIdle(m_LogicalDevice);

		vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
		vkDestroyCommandPool(m_LogicalDevice, m_ComputeCommandPool, nullptr);
		vkctx.deinit();
	}

	namespace Utils
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
				VkSurfaceKHR surface = VulkanContext::GetSwapChain()->GetSurface();
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

	QueueFamilyIndices VulkanDevice::FindQueueFamilies(const VkPhysicalDevice& device)
	{
		//VkSurfaceKHR surface = VulkanContext::GetSwapChain()->GetSurface();

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

			if (indices.isComplete()) {
				break;
			}

			i++;
		}


		return indices;


	}



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
			uint32_t graphicsFamilyIndex = Utils::GetQueueFamilyIndex(m_PhysicalDevice, VK_QUEUE_GRAPHICS_BIT);

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
			uint32_t computeFamilyIndex = Utils::GetQueueFamilyIndex(m_PhysicalDevice, VK_QUEUE_COMPUTE_BIT);

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
			uint32_t transferFamilyIndex = Utils::GetQueueFamilyIndex(m_PhysicalDevice, VK_QUEUE_TRANSFER_BIT);
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

		FROST_VKCHECK(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_LogicalDevice));
	}


	static uint32_t GetPresentQueue(VkPhysicalDevice device)
	{
		VkSurfaceKHR surface = VulkanContext::GetSwapChain()->GetSurface();

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

	VkCommandBuffer VulkanDevice::AllocateCommandBuffer(RenderQueueType queueType, bool beginRecording)
	{
		VkCommandBuffer cmdBuf;

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = queueType == RenderQueueType::Graphics ? m_CommandPool : m_ComputeCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		FROST_VKCHECK(vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &cmdBuf));

		if (beginRecording)
		{
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			FROST_VKCHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo));
		}

		return cmdBuf;
	}

	void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, RenderQueueType queueType)
	{
		VkQueue graphicsQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Queue;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pCommandBuffers = &commandBuffer;
		submitInfo.commandBufferCount = 1;

		// Create fence to ensure that the command buffer has finished executing
		VkFence fence;
		VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		FROST_VKCHECK(vkCreateFence(m_LogicalDevice, &fenceCreateInfo, nullptr, &fence));
		FROST_VKCHECK(vkWaitForFences(m_LogicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));
		vkResetFences(device, 1, &fence);

		// Submit to the queue
		FROST_VKCHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence));
		// Wait for the fence to signal that command buffer has finished executing
		FROST_VKCHECK(vkWaitForFences(m_LogicalDevice, 1, &fence, VK_TRUE, UINT64_MAX));

		vkDestroyFence(m_LogicalDevice, fence, nullptr);

		VkCommandPool commandPool = queueType == RenderQueueType::Graphics ? m_CommandPool : m_ComputeCommandPool;
		vkFreeCommandBuffers(m_LogicalDevice, commandPool, 1, &commandBuffer);
	}

	void VulkanDevice::Init(VkInstance& instance, VkDebugUtilsMessengerEXT& dbMessenger)
	{

		nvvk::ContextCreateInfo contextInfo;
		contextInfo.setVersion(1, 2);
		//contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);
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

		contextInfo.addDeviceExtension(VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME);

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
		m_LogicalDevice = vkctx.getDevice();

		instance = vkctx.getInstance();
		dbMessenger = vkctx.getDbMessenger();


		vk::DynamicLoader dl;
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_LogicalDevice);

		// Getting the queues
		QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
		m_QueueFamilies.GraphicsFamily.Index = indices.graphicsFamily.value();
		m_QueueFamilies.ComputeFamily.Index = indices.computeFamily.value();
		vkGetDeviceQueue(m_LogicalDevice, indices.graphicsFamily.value(), 0, &m_QueueFamilies.GraphicsFamily.Queue);
		vkGetDeviceQueue(m_LogicalDevice, indices.computeFamily.value(), 0, &m_QueueFamilies.ComputeFamily.Queue);

		{
			// Creating the graphics comamnd pool
			VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.queueFamilyIndex = m_QueueFamilies.GraphicsFamily.Index;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			FROST_VKCHECK(vkCreateCommandPool(m_LogicalDevice, &cmdPoolInfo, nullptr, &m_CommandPool));
		}
		{
			// Creating the compute comamnd pool
			VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.queueFamilyIndex = m_QueueFamilies.ComputeFamily.Index;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			FROST_VKCHECK(vkCreateCommandPool(m_LogicalDevice, &cmdPoolInfo, nullptr, &m_ComputeCommandPool));
		}
	}

	static vkb::PhysicalDevice s_Handle;

	void VulkanDevice::Init(const Scope<VulkanPhysicalDevice>& physicalDevice)
	{
		vkb::DeviceBuilder deviceBuilder(s_Handle);
		
		auto device = deviceBuilder.build();

		m_LogicalDevice = device.value().device;
		m_PhysicalDevice = physicalDevice->m_PhysicalDevice;


	}

	VulkanPhysicalDevice::VulkanPhysicalDevice(vkb::Instance vkbInstance)
	{

		m_RequiredPhysicalDeviceExtensions =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
			VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
			VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,

			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
			VK_KHR_MAINTENANCE3_EXTENSION_NAME,
			VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
			VK_KHR_SHADER_CLOCK_EXTENSION_NAME
		};

		vkb::PhysicalDeviceSelector physicalDeviceSelector { vkbInstance };
		//physicalDeviceSelector.set_minimum_version(1, 2);
		//physicalDeviceSelector.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);
		//
		//physicalDeviceSelector.require_dedicated_compute_queue();
		//physicalDeviceSelector.require_dedicated_transfer_queue();
		//physicalDeviceSelector.require_present();
		
		// Should conditionally add these feature, but heck, who's going to use this besides me.
		{
			VkPhysicalDeviceFeatures deviceFeatures;
			deviceFeatures.shaderInt64 = true,
			physicalDeviceSelector.set_required_features(deviceFeatures);


			VkPhysicalDeviceBufferDeviceAddressFeatures deviceAddressFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
			deviceAddressFeatures.bufferDeviceAddress = true,
			physicalDeviceSelector.add_required_extension_features(deviceAddressFeatures);


			VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
			rayTracingPipelineFeatures.rayTracingPipeline = true,
			physicalDeviceSelector.add_required_extension_features(rayTracingPipelineFeatures);


			VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
			accelerationStructureFeatures.accelerationStructure = true,
			physicalDeviceSelector.add_required_extension_features(accelerationStructureFeatures);

			VkPhysicalDeviceDiagnosticsConfigFeaturesNV diagnosticsConfigFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DIAGNOSTICS_CONFIG_FEATURES_NV };
			diagnosticsConfigFeatures.diagnosticsConfig = true,
			physicalDeviceSelector.add_required_extension_features(diagnosticsConfigFeatures);

			VkPhysicalDeviceTimelineSemaphoreFeatures timelineSemaphoreFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES };
			timelineSemaphoreFeatures.timelineSemaphore = true,
			physicalDeviceSelector.add_required_extension_features(timelineSemaphoreFeatures);

			VkPhysicalDeviceScalarBlockLayoutFeatures scalarBlockLayoutFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES };
			scalarBlockLayoutFeatures.scalarBlockLayout = true,
			physicalDeviceSelector.add_required_extension_features(scalarBlockLayoutFeatures);

			VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
			descriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = true,
			descriptorIndexingFeatures.runtimeDescriptorArray = true,
			physicalDeviceSelector.add_required_extension_features(descriptorIndexingFeatures);

			VkPhysicalDeviceShaderClockFeaturesKHR shaderClockFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR };
			shaderClockFeatures.shaderSubgroupClock = true;
			shaderClockFeatures.shaderDeviceClock = true;
			physicalDeviceSelector.add_required_extension_features(shaderClockFeatures);
		}

		// Let vk-bootstrap select our physical device.
		//m_Handle = physicalDeviceSelector.select().result();
		auto physicalDevice = physicalDeviceSelector.select();
		//s_Handle = physicalDevice.value();

		m_PhysicalDevice = physicalDevice.value().physical_device;;
	}

	VulkanPhysicalDevice::~VulkanPhysicalDevice()
	{
		// TODO: Deletion?
	}

}