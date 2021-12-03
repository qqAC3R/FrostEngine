#pragma once

#include <Frost.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"

#include "Frost/Events/KeyEvent.h"
#include "Frost/InputCodes/KeyCodes.h"

namespace Frost
{

	class EditorLayer : public Layer
	{
	public:
		EditorLayer()
			: Layer("Example"), m_Camera(70.0f, 1600.0f / 900.0f, 0.1f, 1000.0f)
		{
		}

		~EditorLayer()
		{
		}

		virtual void OnAttach()
		{
			m_BunnyMesh = Mesh::Load("Resources/Meshes/Sponza/Sponza.gltf", { glm::vec3(1.0f), glm::vec3(0.0f), 0.3f, 1.0f });
			//m_BunnyMesh = Mesh::Load("Resources/Meshes/BistroExterior.fbx", { glm::vec3(1.0f), glm::vec3(0.0f), 1.0f, 1.0f });
			m_PlaneMesh = Mesh::Load("Resources/Meshes/plane.obj", { glm::vec3(1.0f), glm::vec3(0.0f), 1.0f, 1.0f });
			m_SphereMesh = Mesh::Load("Resources/Meshes/Sphere.fbx", { glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, 1.0f });
		}

		virtual void OnUpdate(Frost::Timestep ts)
		{
			m_Camera.OnUpdate(ts);

			Renderer::BeginScene(m_Camera);

			m_SphereMesh->GetVulkanMaterial()[0]->Set("u_MaterialUniform.Metalness", m_Metalness);
			m_SphereMesh->GetVulkanMaterial()[0]->Set("u_MaterialUniform.Roughness", m_Roughness);

			
			glm::mat4 planeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
			//Renderer::Submit(m_PlaneMesh, planeTransform);

			glm::mat4 bunnyTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
			Renderer::Submit(m_BunnyMesh, bunnyTransform);
			
			//bunnyTransform = bunnyTransform * glm::rotate(glm::mat4(1.0f), glm::radians(270.0f), glm::vec3(1, 0, 0));
			//Renderer::Submit(m_BunnyMesh, bunnyTransform);

			glm::mat4 sphereTransform = glm::translate(glm::mat4(1.0f), m_VikingMeshPosition) * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));;
			Renderer::Submit(m_SphereMesh, sphereTransform);

			Renderer::EndScene();

			if (m_LoadCubemap)
			{
				Renderer::LoadEnvironmentMap("Resources/EnvironmentMaps/hilly_terrain_01_4k.hdr");
				m_LoadCubemap = false;
			}
		}


		virtual void OnEvent(Event& event)
		{
			m_Camera.OnEvent(event);
		}

		virtual void OnImGuiRender()
		{
			Renderer::Submit([&]() mutable
			{
				bool showWindow = true;
				static bool dockSpaceOpen = true;
				static bool opt_fullscreen = true;
				static bool opt_padding = false;

				static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

				ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
				if (opt_fullscreen)
				{
					ImGuiViewport* viewport = ImGui::GetMainViewport();
					ImGui::SetNextWindowPos(viewport->WorkPos);
					ImGui::SetNextWindowSize(viewport->WorkSize);
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


				ImGui::Begin("DockSpace", &dockSpaceOpen, window_flags);

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
							Application::Get().Close();

						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}

				/* Rendering the UI over the quad */
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
				ImGui::Begin("Viewport");

				bool IsViewPortFocused = ImGui::IsWindowFocused();
				Application::Get().GetImGuiLayer()->BlockEvents(!IsViewPortFocused);


				ImVec2 viewPortSizePanel = ImGui::GetContentRegionAvail();
				if (m_ViewPortSize.x != viewPortSizePanel.x || m_ViewPortSize.y != viewPortSizePanel.y)
				{
					Renderer::Resize(viewPortSizePanel.x, viewPortSizePanel.y);
					m_Camera.SetViewportSize(viewPortSizePanel.x, viewPortSizePanel.y);
					m_Camera.SetProjectionMatrix(glm::perspective(70.0f, viewPortSizePanel.x / viewPortSizePanel.y, 0.1f, 1000.0f));
					m_ViewPortSize = viewPortSizePanel;
				}

				Ref<Image2D> texture;
				if (m_UseRT)
					texture = Renderer::GetFinalImage(0);
				else
					texture = Renderer::GetFinalImage(1);

				ImGui::Image(ImGuiLayer::GetTextureIDFromVulkanTexture(texture), ImVec2{ viewPortSizePanel.x, viewPortSizePanel.y });


				ImGui::End();
				ImGui::PopStyleVar();


				UI::Begin("Scene");
				UI::Slider("Entity Position", m_VikingMeshPosition, -20, 20);
				UI::Slider("Roughness", m_Roughness, 0.0f, 0.99f);
				UI::Slider("Metalness", m_Metalness, 0.0f, 0.99f);
				UI::End();

				UI::Begin("Settings");
				UI::CheckMark("RayTracing", m_UseRT);
				//UI::CheckMark("LoadCubemap", m_LoadCubemap);

				ImGui::Separator();
				ImGui::Text("Camera Properties");
				UI::Slider("Exposure", m_Camera.GetExposure(), 0.0f, 5.0f);
				UI::Slider("DOF", m_Camera.GetDOF(), 0.0f, 5.0f);
				UI::End();

				ImGui::End();
			});
		}

		virtual void OnResize()
		{
		}

		virtual void OnDetach()
		{
			this->~EditorLayer();
		}

	private:

		EditorCamera m_Camera;

		glm::vec3 m_CameraPos{ 0.0f, 2.0f, -0.2f };
		glm::vec3 m_LightPos{ 1.0f, 1.0f, 1.0f };
		float m_SphereRoughness = 0.0f;

		float m_Roughness = 0.0f;
		float m_Metalness = 1.0f;

		glm::vec3 m_VikingMeshPosition{ 0.5f, 2.0f, -0.2f };
		ImVec2 m_ViewPortSize{};

		Ref<Mesh> m_PlaneMesh;
		Ref<Mesh> m_BunnyMesh;
		Ref<Mesh> m_SphereMesh;

		bool m_UseRT = false;
		bool m_LoadCubemap = false;

	};


#if 0
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();

		virtual void OnAttach();
		virtual void OnUpdate(Frost::Timestep ts);
		virtual void OnDetach();

		virtual void OnEvent(Frost::Event& event);
		virtual void OnImGuiRender();
		virtual void OnResize();

	private:

		EditorCamera m_Camera;

		glm::vec3 m_CameraPos{ 1.0f, 1.0f, 1.0f };
		glm::vec3 m_LightPos{ 1.0f, 1.0f, 1.0f };

		glm::vec3 m_VikingMeshPosition{ 2.0f };

		Ref<Mesh> m_PlaneMesh;
		Ref<Mesh> m_VikingMesh;

	};
#endif
}