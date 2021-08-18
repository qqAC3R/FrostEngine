#if 0
#include "frostpch.h"
#include "EditorLayer.h"

#include "imgui.h"

namespace Frost
{



	EditorLayer::EditorLayer()
		: Layer("Example"), m_Camera(70, 16.0f / 9.0f, 0.1f, 100.0f, Frost::CameraMode::ORBIT)
	{

	}

	void EditorLayer::OnAttach()
	{
		m_PlaneMesh = Mesh::Load("assets/media/scenes/plane.obj");
		m_VikingMesh = Mesh::Load("assets/models/stanford-bunny.fbx");
	}

	void EditorLayer::OnUpdate(Frost::Timestep ts)
	{
		m_Camera.OnUpdate(ts);

		Renderer::BeginRenderPass(m_Camera);

		glm::mat4 planeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
		Renderer::Submit(m_PlaneMesh, planeTransform);


		glm::mat4 vikingTransform = glm::translate(glm::mat4(1.0f), m_VikingMeshPosition) * glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
		Renderer::Submit(m_VikingMesh, vikingTransform);



		Renderer::EndRenderPass();
	}

	void EditorLayer::OnDetach()
	{
		m_PlaneMesh->Destroy();
		m_VikingMesh->Destroy();
	}

	void EditorLayer::OnEvent(Frost::Event& event)
	{
		m_Camera.OnEvent(event);
	}

	void EditorLayer::OnImGuiRender()
	{
		bool showWindow = true;
		ImGui::ShowDemoWindow(&showWindow);


		static bool dockSpaceOpen = true;

		static bool opt_fullscreen = true;
		static bool opt_padding = false;

		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->GetWorkPos());
			ImGui::SetNextWindowSize(viewport->GetWorkSize());
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
			dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		if (!opt_padding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));


		ImGui::Begin("DockSpace Demo", &dockSpaceOpen, window_flags);
		if (!opt_padding)
			ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);


		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					//Application::Get().Close();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}


		Frost::UI::Begin("Window");
		Frost::UI::End();

		ImGui::End();
	}

	void EditorLayer::OnResize()
	{

	}

}
#endif