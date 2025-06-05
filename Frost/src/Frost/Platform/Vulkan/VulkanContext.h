#pragma once

#include "Frost/Renderer/GraphicsContext.h"
#include "VulkanSwapChain.h"
#include "VulkanDevice.h"

namespace Frost
{
	class VulkanSemaphore;
	
	class VulkanContext : public GraphicsContext
	{
	public:
		VulkanContext(GLFWwindow* window);
		virtual ~VulkanContext();

		virtual void Init() override;
		virtual void Resize(uint32_t width, uint32_t height) override;

		/* Vulkan Specific */
		virtual WindowDimension GetWindowDimension() override { return WindowDimension(); }

		static const Scope<VulkanSwapChain>& GetSwapChain() { return m_SwapChain; }
		static const Scope<VulkanDevice>& GetCurrentDevice() { return m_LogicalDevice; }
		static VkPipelineBindPoint GetVulkanGraphicsType(GraphicsType type);
		static VkInstance GetInstance() { return m_Instance; }

		virtual void WaitDevice() const override;

		virtual GPUMemoryStats GetGPUMemoryStats() const override;

		template <typename T>
		static void SetStructDebugName(const std::string& name, VkObjectType type, T handle)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			VkDebugUtilsObjectNameInfoEXT debugName{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
			debugName.pNext = nullptr;
			debugName.objectType = type;
			debugName.pObjectName = name.c_str();
			debugName.objectHandle = (uint64_t)handle;
#if FROST_DEBUG
			vkSetDebugUtilsObjectNameEXT(device, &debugName);
#endif
		}

		static bool ValidationLayersEnabled() { return m_EnableValidationLayers; }

	private:
		void CreateInstance();
		void CreateDebugMessenger();
		void LoadVulkanFunctionPointers();


	private:
		GLFWwindow* m_Window;

		static VkInstance m_Instance;
		static VkDebugUtilsMessengerEXT m_DebugMessenger;
		static bool m_EnableValidationLayers;

		static uint32_t m_VulkanAPIVersion;

		static Scope<VulkanDevice> m_LogicalDevice;
		static Scope<VulkanPhysicalDevice> m_PhysicalDevice;
		static Scope<VulkanSwapChain> m_SwapChain;

		Vector<const char*> m_SupportedInstanceExtensions;

	public:
		struct Settings
		{
			bool Validation = false;
			bool Fullscreen = false;
			bool Vsync = false;
		};
	private:
		Settings m_Settings;
	};


}