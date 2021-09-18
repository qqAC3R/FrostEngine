#include "frostpch.h"
#include "Application.h"

#include "Frost/Utils/Timer.h"

#include "Frost/Renderer/Renderer.h"

#include "Frost/Core/Input.h"
#include "Frost/InputCodes/MouseButtonCodes.h"

#include <GLFW/glfw3.h>

namespace Frost
{
	
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		FROST_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;
		
		FROST_CORE_INFO("Frost Engine v{0} (Beta aka 'im dying send help') ", 0.2f);

		m_Window = Scope<Window>(Window::Create({"Frost Engine", 1600, 900}));
		m_Window->SetEventCallback(FROST_BIND_EVENT_FN(Application::OnEvent));

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);

		Renderer::Init();
	}


	Application::~Application()
	{
		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
		}

		Renderer::ShutDown();
	}

	void Application::Run()
	{
		while (m_Running)
		{
			m_Window->OnUpdate();
			
			// Timestep
			float time = (float)m_Window->GetTime(); // Platform::GetTime
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			if (!m_Minimized)
			{
				Renderer::BeginFrame();

				// ImGui
				m_ImGuiLayer->Begin();
				for (Layer* layer : m_LayerStack)
				{
					layer->OnImGuiRender();
				}


				// Update
				for (Layer* layer : m_LayerStack)
				{
					layer->OnUpdate(timestep);
				}

				Renderer::EndFrame();

				Renderer::ExecuteCommandBuffer();
			}

			// Poll Events
			m_Window->OnUpdate();

		}

		m_Window->GetGraphicsContext()->WaitDevice();

	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(FROST_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(FROST_BIND_EVENT_FN(Application::OnWindowResize));


		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			// Check if a layer is blocking the events
			if ((*--it)->IsBlocking())
			{
				(*it)->OnEvent(e);
				if (e.Handled)
					break;
			}
		}

	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}
		m_Minimized = false;
		
		m_Window->Resize(e.GetWidth(), e.GetHeight());
		Renderer::Resize(e.GetWidth(), e.GetHeight());
		m_ImGuiLayer->OnResize(e.GetWidth(), e.GetHeight());

		return false;
	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
		layer->OnAttach();
	}

	void Application::PushOverlay(Layer* layer)
	{
		m_LayerStack.PushOverlay(layer);
		layer->OnAttach();
	}

}