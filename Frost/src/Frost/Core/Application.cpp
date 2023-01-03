#include "frostpch.h"
#include "Application.h"

#include "Frost/Utils/Timer.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Physics/PhysicsEngine.h"

#include "Frost/Core/Input.h"
#include "Frost/InputCodes/MouseButtonCodes.h"
#include "Frost/Utils/Timer.h"

#include <GLFW/glfw3.h>

namespace Frost
{
	Application* Application::s_Instance = nullptr;
	std::string Application::m_ApplicationVersion = "0.5.0a";

	Application::Application()
	{
		FROST_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;
		
		FROST_CORE_INFO("Frost Engine v{0}", Application::GetVersion());

		m_Window = Scope<Window>(Window::Create({"Frost Engine", 1600, 900}));
		m_Window->SetWindowName("Frost");
		m_Window->SetEventCallback(FROST_BIND_EVENT_FN(Application::OnEvent));

		m_ImGuiLayer = ImGuiLayer::Create();
		PushOverlay(m_ImGuiLayer);

		Renderer::Init();

		PhysicsEngine::Initialize();
	}

	Application::~Application()
	{
		for (Layer* layer : m_LayerStack)
		{
			layer->OnDetach();
		}
		PhysicsEngine::ShutDown();
		Renderer::ShutDown();
	}

	void Application::Run()
	{
		while (m_Running)
		{
			// Poll Events
			m_Window->OnUpdate();

			// Timestep
			float time = (float)m_Window->GetTime(); // Platform::GetTime
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;
			
			if (!m_Minimized)
			{
				Renderer::BeginFrame();

				// Update
				for (Layer* layer : m_LayerStack)
				{
					layer->OnUpdate(timestep);
				}

				// ImGui
				m_ImGuiLayer->Begin(); // Send to the queue? + ImGui stuff in the editor layer??
				for (Layer* layer : m_LayerStack)
				{
					layer->OnImGuiRender();
				}

				Renderer::SubmitCmdsToRender(); // Submit commands to the graphics queue

				Renderer::EndFrame(); // Present

				// Execute the RenderCommandBuffers (pointer functions)
				Renderer::ExecuteCommandBuffer();


				Renderer::ExecuteDeletionCommands();
			}
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
		const uint32_t width = e.GetWidth(), height = e.GetHeight();
		if (width == 0 || height == 0)
		{
			m_Minimized = true;
			return false;
		}
		m_Minimized = false;
		
		m_Window->Resize(width, height);
		m_ImGuiLayer->OnResize(width, height);

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