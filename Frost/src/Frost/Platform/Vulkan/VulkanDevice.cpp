#include "frostpch.h"
#include "VulkanDevice.h"
#include "Frost/Core/Engine.h"

#include "VulkanContext.h"

namespace Frost
{

	VulkanPhysicalDevice::VulkanPhysicalDevice(VkInstance instance)
	{
		PickPhysicalDevice(instance);
	}

	VulkanPhysicalDevice::~VulkanPhysicalDevice()
	{

	}

	static bool HasAllFeatures(VkPhysicalDevice device)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		return (
			deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && // Must be a dedicated GPU
			deviceFeatures.geometryShader == VK_TRUE && // Must have geometry shader for voxelization
			deviceFeatures.multiDrawIndirect == VK_TRUE && // Must have multi draw indirect
			deviceFeatures.samplerAnisotropy == VK_TRUE    // Must have sampler anisotropy
		);
	}

	static QueueFamilies FindQueueFamilies(VkPhysicalDevice phyiscalDevice)
	{
		QueueFamilies indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(phyiscalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(phyiscalDevice, &queueFamilyCount, queueFamilies.data());


		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.GraphicsFamily.Index = i;
			}

			if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
				indices.ComputeFamily.Index = i;
			}

			if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
				indices.TransferFamily.Index = i;
			}

			if (indices.GraphicsFamily && indices.ComputeFamily && indices.TransferFamily)
			{
				break;
			}

			i++;
		}

		// Logic to find queue family indices to populate struct with
		return indices;
	}

	static bool HasAllQueuesNeeded(VkPhysicalDevice phyiscalDevice)
	{
		QueueFamilies indices = FindQueueFamilies(phyiscalDevice);
		return indices.GraphicsFamily && indices.ComputeFamily && indices.TransferFamily;
	}

	void VulkanPhysicalDevice::PickPhysicalDevice(VkInstance instance)
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			FROST_CORE_WARN("!!No GPUs Available!!");
		};

		std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

		for (const auto& physicalDevice : physicalDevices)
		{
			if (HasAllQueuesNeeded(physicalDevice) && HasAllFeatures(physicalDevice))
			{
				m_PhysicalDevice = physicalDevice;
				break;
			}
		}

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
		FROST_CORE_INFO("Picked Physical Device: {0}", deviceProperties.deviceName);
	}




	VulkanDevice::VulkanDevice()
	{
	}

	VulkanDevice::~VulkanDevice()
	{
	}


	void VulkanDevice::Init(const Scope<VulkanPhysicalDevice>& physicalDevice)
	{
		m_PhysicalDevice = physicalDevice->GetVulkanPhysicalDevice();
		CreateLogicalDevice(m_PhysicalDevice);

		{
			// Creating the graphics comamnd pool
			VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.queueFamilyIndex = m_FamilyQueues.GraphicsFamily.Index;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			FROST_VKCHECK(vkCreateCommandPool(m_LogicalDevice, &cmdPoolInfo, nullptr, &m_CommandPool));
		}
		{
			// Creating the compute comamnd pool
			VkCommandPoolCreateInfo cmdPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			cmdPoolInfo.queueFamilyIndex = m_FamilyQueues.ComputeFamily.Index;
			cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			FROST_VKCHECK(vkCreateCommandPool(m_LogicalDevice, &cmdPoolInfo, nullptr, &m_ComputeCommandPool));
		}
	}

	void VulkanDevice::CreateLogicalDevice(VkPhysicalDevice physicalDevice)
	{
		m_FamilyQueues = FindQueueFamilies(physicalDevice);

		uint32_t familyIndicesCount = 0;
		if (m_FamilyQueues.ComputeFamily.Index != m_FamilyQueues.GraphicsFamily.Index ||
			m_FamilyQueues.ComputeFamily.Index != m_FamilyQueues.TransferFamily.Index)
		{
			familyIndicesCount++;
		}

		if (m_FamilyQueues.GraphicsFamily.Index != m_FamilyQueues.ComputeFamily.Index ||
			m_FamilyQueues.GraphicsFamily.Index != m_FamilyQueues.TransferFamily.Index)
		{
			familyIndicesCount++;
		}


		if (m_FamilyQueues.TransferFamily.Index != m_FamilyQueues.ComputeFamily.Index ||
			m_FamilyQueues.TransferFamily.Index != m_FamilyQueues.ComputeFamily.Index)
		{
			familyIndicesCount++;
		}

		// If all the queues have the same index, then we need only a device queue
		if (familyIndicesCount == 0)
			familyIndicesCount = 1;


		float queuePriority = 1.0f;
		std::vector<VkDeviceQueueCreateInfo> queuesCreateInfo(familyIndicesCount);
		for (auto& queueCreateInfo : queuesCreateInfo)
		{
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = m_FamilyQueues.ComputeFamily.Index;
			queueCreateInfo.queueCount = familyIndicesCount;

			queueCreateInfo.pQueuePriorities = &queuePriority;
		}



		const std::vector<const char*> validationLayers = {
				"VK_LAYER_KHRONOS_validation"
		};

		const std::vector<const char*> extensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME, // Swapchain
			VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
			VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
			VK_KHR_RELAXED_BLOCK_LAYOUT_EXTENSION_NAME, // It removes the errors for the scalar blocks
			VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME, // It removes the errors for the scalar blocks

			VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, // Bindless
			VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, // Scalar block
			VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, // BDA

			VK_EXT_SAMPLER_FILTER_MINMAX_EXTENSION_NAME, // Hi-Z sampling

			VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME, // Voxelization

			VK_KHR_SHADER_CLOCK_EXTENSION_NAME, // Random numbers

#if 1
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // RT
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,   // RT
			VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME	    // RT
#endif
		};

		VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		createInfo.pQueueCreateInfos = queuesCreateInfo.data();
		createInfo.queueCreateInfoCount = queuesCreateInfo.size();

		// Device features

		VkPhysicalDeviceVulkan12Features features12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		features12.descriptorIndexing = true;
		features12.imagelessFramebuffer = true;
		features12.hostQueryReset = true;
		features12.runtimeDescriptorArray = true;
		features12.samplerFilterMinmax = true;
		features12.scalarBlockLayout = true;
		features12.descriptorBindingUpdateUnusedWhilePending = true;
		features12.descriptorBindingPartiallyBound = true;
		features12.shaderSampledImageArrayNonUniformIndexing = true;
		features12.bufferDeviceAddress = true;

		VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features.features.wideLines = true;
		features.features.textureCompressionBC = true;
		features.features.independentBlend = true;
		features.features.fillModeNonSolid = true;
		features.features.multiDrawIndirect = true;
		features.features.samplerAnisotropy = true;
		features.features.shaderStorageImageWriteWithoutFormat = true;
		features.features.geometryShader = true;
		features.features.fragmentStoresAndAtomics = true;
		features.features.shaderInt64 = true;

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
		rayTracingFeatures.rayTracingPipeline = true;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		accelerationStructFeatures.accelerationStructure = true;


		// Creating the chain
		features12.pNext = &features;
		features.pNext = &rayTracingFeatures;
		rayTracingFeatures.pNext = &accelerationStructFeatures;
		accelerationStructFeatures.pNext = nullptr;

		createInfo.pNext = &features12;
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (VulkanContext::ValidationLayersEnabled())
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		// FROST_VKCHECK
		FROST_VKCHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &m_LogicalDevice));

		// Get the device queues
		vkGetDeviceQueue(m_LogicalDevice, m_FamilyQueues.GraphicsFamily.Index, 0, &m_FamilyQueues.GraphicsFamily.Queue);
		vkGetDeviceQueue(m_LogicalDevice, m_FamilyQueues.ComputeFamily.Index, 0, &m_FamilyQueues.ComputeFamily.Queue);
		vkGetDeviceQueue(m_LogicalDevice, m_FamilyQueues.TransferFamily.Index, 0, &m_FamilyQueues.TransferFamily.Queue);
	}


	void VulkanDevice::ShutDown()
	{
		vkDeviceWaitIdle(m_LogicalDevice);

		vkDestroyCommandPool(m_LogicalDevice, m_CommandPool, nullptr);
		vkDestroyCommandPool(m_LogicalDevice, m_ComputeCommandPool, nullptr);
		vkDestroyDevice(m_LogicalDevice, nullptr);
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
}