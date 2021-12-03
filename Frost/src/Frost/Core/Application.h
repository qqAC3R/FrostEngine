#pragma once

#include "Frost/Core/Engine.h"
#include "Frost/Core/Window.h"
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
		void Close() { m_Running = false; }
		
		void OnEvent(Event& e);
		
		bool IsClosed() { return m_Running; }
		bool IsMinimized() { return m_Minimized; }

		inline Window& GetWindow() { return *m_Window; }
		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* layer);

		inline static Application& Get() { return *s_Instance; }
		inline static const std::string& GetVersion() { return m_ApplicationVersion; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);
	private:
		Scope<Window> m_Window;
		LayerStack m_LayerStack;
		ImGuiLayer* m_ImGuiLayer;

		bool m_Running = true;
		bool m_Minimized = false;
		float m_LastFrameTime = 0.0f;
	private:
		static Application* s_Instance;
		static std::string m_ApplicationVersion;
	};

	Application* CreateApplication();
}