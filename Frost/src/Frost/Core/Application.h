#pragma once

//#include <vulkan/vulkan.hpp>

#include "Frost/Core/Engine.h"
#include "Frost/Core/Window.h"
//#include "Frost/Core/Device.h"
#include "Frost/Core/LayerStack.h"

#include "Frost/Events/ApplicationEvent.h"
#include "Frost/Events/Event.h"

#include "Frost/ImGui/ImGuiLayer.h"


namespace Frost
{


	class Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
		
		void OnEvent(Event& e);
		
		bool IsClosed() { return m_Running; }
		bool IsMinimized() { return m_Minimized; }
		
		//void AddExtension(const char* extension);

		inline Window& GetWindow() { return *m_Window; }

		//inline Scope<Window>* GetWindowPtr() { return &m_Window; }
		//inline VulkanDevice& GetVulkanDevice() { return *m_Device; }
		//
		//inline Scope<VulkanDevice>* GetVulkanDevicePtr() { return &m_Device; }



		//VkSurfaceKHR& GetSurface() { return m_Surface; }
		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);
	
		inline static Application& Get() { return *s_Instance; }
		//inline ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		//static void WaitDevice();

		//static void BeginImGui();
		//static void RenderImGui(VkCommandBuffer cmdBuf);

	private:
		//void CreateInstance();
		//void CreateSurface();
		//void SetupDebugMessenger();

		//void ReacreateSwapChain();

		//bool CheckValidationLayerSupport();

		//std::vector<const char*> GetRequiredExtensions();

	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		Scope<Window> m_Window;
		//VkInstance m_Instance;
		//VkDebugUtilsMessengerEXT m_DebugMessenger;
		//VkSurfaceKHR m_Surface;

		//Scope<VulkanDevice> m_Device;

		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer;

		//const std::vector<const char*> m_ValidationLayers = {
		//	"VK_LAYER_KHRONOS_validation",
		//};
		//std::vector<const char*> m_DeviceExtension = {
		//	//VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		//	//VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		//	//VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME
		//};

		//bool enableValidationLayers = true;

		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;

	private:
		static Application* s_Instance;

	};

	Application* CreateApplication();
}