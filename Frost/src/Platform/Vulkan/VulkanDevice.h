#pragma once

#include <vulkan/vulkan.hpp>

#include "nvvk/context_vk.hpp"

namespace Frost
{

	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		std::optional<uint32_t> computeFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value();
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
			uint32_t Index;
			VkQueue Queue;
		};

		QueueFamily GraphicsFamily;
		QueueFamily PresentFamily;
		QueueFamily ComputeFamily;
		QueueFamily TransferFamily;
	};


	class VulkanDevice
	{
	public:
		VulkanDevice();
		virtual ~VulkanDevice();

		//void Init(VkInstance& instance, VkDebugUtilsMessengerEXT& dbMessenger);
		void Init(VkInstance& instance, VkDebugUtilsMessengerEXT& dbMessenger);
		virtual void ShutDown();



		VkDevice GetVulkanDevice() const { return m_Device; }
		VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }


		//operator VkDevice() const { return m_Device; }

		QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device);

		//const VkQueueMembers& GetQueues() { return m_Queues; }
		const QueueFamilies& GetQueueFamilies() { return m_QueueFamilies; }
		void SetQueues(VkSurfaceKHR surface);

		VkFormat FindDepthFormat();


		//const VkInstance& GetInstance() const { return *m_Instance; }

	private:



		//uint32_t GetQueueFamilyIndex(VkQueueFlagBits queueFlags) const;
		void CreateDevice(std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes);

		//void CreateInstance(VkApplicationInfo& applicationInfo);

		bool ExtensionSupported(std::string extension) { return false; }

		//int RateDevice(const VkPhysicalDevice& device);


		//bool IsDeviceSuitable(const VkPhysicalDevice& device);
		//bool CheckDeviceExtensionSupport(const VkPhysicalDevice& device);
		

		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

	private:
		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_Device;

		//VkQueueMembers m_Queues;
		//Vector<VkQueueFamilyProperties> m_QueueFamilyProperties;
		QueueFamilies m_QueueFamilies{};


		//VkInstance m_Instance;
		//VkSurfaceKHR m_Surface;

		//Vector<const char*> m_EnabledDeviceExtensions;
		//Vector<std::string> m_SupportedExtensions;
		//Vector<std::string> m_SupportedInstanceExtensions;

		//VkPhysicalDeviceFeatures m_AvailableFeatures;
		//VkPhysicalDeviceFeatures m_EnabledFeatures;



		/// This part is a bit tricky because to get some extensions you need the give vulkan their special features
		// Enabled features and properties
		//VkPhysicalDeviceBufferDeviceAddressFeatures m_BufferDeviceAddresFeatures{};
		//VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_RayTracingPipelineFeatures{};
		//VkPhysicalDeviceAccelerationStructureFeaturesKHR m_AccelerationStructureFeatures{};
		//void* m_pNextChain = nullptr;



		//PFN_vkDebugUtilsMessengerCallbackEXT m_DebuggerCallBack;

		//std::vector<const char*> m_DeviceExtension = {
		//	//VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		//	//VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		//	//VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
		//};

		//uint32_t m_VulkanApiVersion = VK_API_VERSION_1_2;


		//struct Settings
		//{
		//	bool Validation = false;
		//	bool Fullscreen = false;
		//	bool Vsync = false;
		//};
		//Settings m_Settings;


		vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature;
		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature;


		nvvk::Context vkctx;
	};

}