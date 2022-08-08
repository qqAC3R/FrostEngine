#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{
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

		void Init(const Scope<VulkanPhysicalDevice>& physicalDevice);
		void ShutDown();

		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }

		const QueueFamilies& GetQueueFamilies() { return m_FamilyQueues; }

		VkCommandBuffer AllocateCommandBuffer(RenderQueueType queueType = RenderQueueType::Graphics, bool beginRecording = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, RenderQueueType queueType = RenderQueueType::Graphics);

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

		friend class VulkanRenderer;
	};

}