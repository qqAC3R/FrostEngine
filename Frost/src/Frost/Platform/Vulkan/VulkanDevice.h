#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

//#include "nvvk/context_vk.hpp"

namespace vkb
{
	struct Instance;
	struct PhysicalDevice;
}

namespace Frost
{

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> computeFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && computeFamily.has_value();
		}
	};

	struct VkQueueMembers
	{
		VkQueue m_GraphicsQueue;
		uint32_t m_GQIndex;
		
		VkQueue m_PresentQueue;
		uint32_t m_PresentIndex;

		VkQueue m_ComputeQueue;
		uint32_t m_ComputeIndex;
	};

	struct QueueFamilies
	{
		struct QueueFamily
		{
			uint32_t Index = UINT32_MAX;
			VkQueue Queue;

			operator bool() const { return Index != UINT32_MAX; }
		};

		QueueFamily GraphicsFamily;
		QueueFamily ComputeFamily;
		QueueFamily TransferFamily;
	};

	enum class RenderQueueType
	{
		Graphics, Compute
	};


	class VulkanPhysicalDevice
	{
	public:
		VulkanPhysicalDevice(VkInstance instance);
		virtual ~VulkanPhysicalDevice();

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
	private:
		void PickPhysicalDevice(VkInstance instance);
	private:
		VkPhysicalDevice m_PhysicalDevice;
	};


	class VulkanDevice
	{
	public:
		VulkanDevice();
		virtual ~VulkanDevice();

		//void Init(VkInstance& instance, VkDebugUtilsMessengerEXT& dbMessenger);
		//void Init(const Scope<VulkanPhysicalDevice>& physicalDevice);
		void Init(const Scope<VulkanPhysicalDevice>& physicalDevice);
		void ShutDown();

		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		//QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device);
		const QueueFamilies& GetQueueFamilies() { return m_FamilyQueues; }

		VkCommandBuffer AllocateCommandBuffer(RenderQueueType queueType = RenderQueueType::Graphics, bool beginRecording = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, RenderQueueType queueType = RenderQueueType::Graphics);

	private:
		//void CreateDevice(std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes);
		//bool ExtensionSupported(std::string extension) { return false; }
		//VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	private:
		void CreateLogicalDevice(VkPhysicalDevice physicalDevice);

	private:
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_LogicalDevice;

		VkCommandPool m_CommandPool;
		VkCommandPool m_ComputeCommandPool;

		QueueFamilies m_FamilyQueues;

		//vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature;
		//vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature;


		//nvvk::Context vkctx;

		friend class VulkanRenderer;
	};

}