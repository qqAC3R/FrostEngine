#pragma once

#include "Frost/Renderer/GraphicsContext.h"

#include "VulkanSwapChain.h"
#include "VulkanDevice.h"

#define FRAMES_IN_FLIGHT 3

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

		/* OpenGL Specific */
		virtual void SwapBuffers() override {}

		/* Vulkan Specific */
		virtual WindowDimension GetWindowDimension() override { return WindowDimension(); }

		static const Scope<VulkanDevice>&	 GetCurrentDevice() { return m_Device; }
		static VkInstance					 GetInstance() { return m_Instance; }
		static const Scope<VulkanSwapChain>& GetSwapChain() { return m_SwapChain; }
		static VkSurfaceKHR					 GetSurface() { return m_Surface; }
		static Ref<RenderPass>				 GetRenderPass() { return m_RenderPass; }

		static VkPipelineBindPoint GetVulkanGraphicsType(GraphicsType type);

		static void ResetSwapChain();
		static std::tuple<VkResult, uint32_t> AcquireNextSwapChainImage(VulkanSemaphore availableSemaphore);
		static void BeginFrame(VkCommandBuffer cmdBuf, uint32_t imageIndex);
		static void EndFrame(VkCommandBuffer cmdbuf);
		static void Present(VkSemaphore waitSemaphore, uint32_t imageIndex);

		virtual void WaitDevice() override;

		template <typename T>
		static void SetStructDebugName(const std::string& name, VkObjectType type, T handle)
		{
			//PFN_vkSetDebugUtilsObjectNameEXT pfn_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(m_Instance, "vkSetDebugUtilsObjectNameEXT");

			
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			VkDebugUtilsObjectNameInfoEXT debugName{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
			debugName.pNext = nullptr;
			debugName.objectType = type;
			debugName.pObjectName = name.c_str();
			debugName.objectHandle = (uint64_t)handle;
			vkSetDebugUtilsObjectNameEXT(device, &debugName);

		}
	private:
		void InitFunctionPointers();

	private:
		GLFWwindow* m_Window;

		static VkInstance m_Instance;
		static VkDebugUtilsMessengerEXT m_DebugMessenger;
		static VkSurfaceKHR m_Surface;
		static uint32_t m_VulkanAPIVersion;

		static Scope<VulkanDevice> m_Device;
		static Scope<VulkanSwapChain> m_SwapChain;
		static Ref<RenderPass> m_RenderPass;

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