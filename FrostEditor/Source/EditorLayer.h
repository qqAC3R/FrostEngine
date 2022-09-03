#pragma once

#include <Frost.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include "Frost/Core/Input.h"
#include "Frost/Events/KeyEvent.h"
#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/InputCodes/MouseButtonCodes.h"

#include "Frost/Math/Math.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Panels/MaterialEditor.h"
#include "Panels/AssetBrowser.h"

#include "UserInterface/UIWidgets.h"

#include "IconsFontAwesome.hpp"

namespace Frost
{
	enum class SceneState
	{
		Edit,
		Play
	};

	class EditorLayer : public Layer
	{
	public:
		EditorLayer()
			: Layer("Editor")
		{
			m_EditorCamera = Ref<EditorCamera>::Create(85.0, 1600.0f / 900.0f, 0.1f, 2000.0f);
		}

		~EditorLayer()
		{
		}

		virtual void OnAttach()
		{
			// Scene initialization
			m_EditorScene = Ref<Scene>::Create();

			// Scene Hierarchy Panel initialization
			m_SceneHierarchyPanel = Ref<SceneHierarchyPanel>::Create();
			m_SceneHierarchyPanel->Init(nullptr);
			m_SceneHierarchyPanel->SetSceneContext(m_EditorScene);

			// Inspector Panel initialization
			m_InspectorPanel = Ref<InspectorPanel>::Create();
			m_InspectorPanel->Init(nullptr);
			m_InspectorPanel->SetHierarchy(m_SceneHierarchyPanel);

			m_ViewportPanel = Ref<ViewportPanel>::Create();
			m_ViewportPanel->Init(nullptr);

			m_MaterialEditor = Ref<MaterialEditor>::Create();
			m_MaterialEditor->Init(nullptr);

			m_AssetBrowser = Ref<AssetBrowser>::Create();
			m_AssetBrowser->Init(nullptr);

			{
				auto& sponzaEntity = m_EditorScene->CreateEntity("Plane");
				auto& meshComponent = sponzaEntity.AddComponent<MeshComponent>();
				meshComponent.Mesh = Mesh::Load("Resources/Meshes/Plane.obj", { glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, 1.0f });
			}

			{
				auto& directionalLight = m_EditorScene->CreateEntity("Directional Light");
				auto& directionalLightComponent = directionalLight.AddComponent<DirectionalLightComponent>();
			}
		}

		virtual void OnUpdate(Timestep ts)
		{
			m_EditorCamera->OnUpdate(ts);

			if (m_SceneState == SceneState::Play)
			{
				CameraComponent* primaryCamera = m_EditorScene->GetPrimaryCamera();

				if (primaryCamera)
				{
					if (m_SceneState == SceneState::Play)
					{
						primaryCamera->Camera->SetViewportSize(m_ViewportPanel->GetViewportPanelSize().x, m_ViewportPanel->GetViewportPanelSize().y);
						Renderer::BeginScene(primaryCamera->Camera);
					}
				}
				else
				{
					Renderer::BeginScene(m_EditorCamera);
				}
			}
			else
			{
				Renderer::BeginScene(m_EditorCamera);
			}

			m_EditorScene->Update(ts);
			Renderer::EndScene();
		}

		virtual void OnImGuiRender()
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
					if (ImGui::MenuItem("Open"))
						OpenScene();
					if (ImGui::MenuItem("Save As"))
						SaveSceneAs();

					ImGui::Separator();
					
					if (ImGui::MenuItem("Exit"))
						Application::Get().Close();

					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

		


			Renderer::Submit([&]()
			{
				Renderer::RenderDebugger();

				// Scene hierarchy panel rendering
				m_SceneHierarchyPanel->Render();

				// Inspector panel rendering
				m_InspectorPanel->Render();

				// Material editor rendering
				m_MaterialEditor->SetActiveEntity(m_SceneHierarchyPanel->GetSelectedEntity());
				m_MaterialEditor->SetActiveSubmesh(m_SceneHierarchyPanel->GetSelectedEntitySubmesh());
				m_MaterialEditor->Render();

				// Asset browser rendering
				m_AssetBrowser->Render();

				// Viewport rendering
				m_ViewportPanel->BeginRender();

				if (m_ViewportPanel->IsResized())
				{
					glm::vec2 viewportPanelSize = m_ViewportPanel->GetViewportPanelSize();

					Renderer::Resize(viewportPanelSize.x, viewportPanelSize.y);
					m_EditorCamera->SetViewportSize(viewportPanelSize.x, viewportPanelSize.y);
					//m_EditorCamera->Resize(viewportPanelSize.x / viewportPanelSize.y);
				}


				Ref<Image2D> texture = Renderer::GetFinalImage(1);
				m_ViewportPanel->RenderTexture(texture);


				{
					Entity selectedEntity = m_SceneHierarchyPanel->GetSelectedEntity();
					if (selectedEntity && m_GuizmoMode != -1 && selectedEntity.HasComponent<TransformComponent>())
					{
						ImGuizmo::SetOrthographic(false);
						ImGuizmo::SetDrawlist();

						float windowWidth = (float)ImGui::GetWindowWidth();
						float windowHeight = (float)ImGui::GetWindowHeight();

						ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, windowWidth, windowHeight);

						// Camera
						const glm::mat4& cameraProjection = m_EditorCamera->GetProjectionMatrix();
						glm::mat4 cameraView = glm::inverse(glm::translate(glm::mat4(1.0f),
							m_EditorCamera->GetPosition()) * glm::toMat4(m_EditorCamera->GetOrientation())
						);


						// Get the world position for the entity, then we will convert it in local position (if it is a child)
						glm::mat4 transform = m_EditorScene->GetTransformMatFromEntityAndParent(selectedEntity);
						TransformComponent& tc = selectedEntity.GetComponent<TransformComponent>();

						// Snapping
						bool snap = Input::IsKeyPressed(Key::LeftControl);
						float snapValue = 0.5f;
						if (m_GuizmoMode == ImGuizmo::OPERATION::ROTATE)
							snapValue = 45.0f;

						float snapValues[3] = { snapValue, snapValue, snapValue };


						ImGuizmo::Manipulate(
							glm::value_ptr(cameraView),
							glm::value_ptr(cameraProjection),
							(ImGuizmo::OPERATION)m_GuizmoMode,
							ImGuizmo::LOCAL,
							glm::value_ptr(transform),
							nullptr,
							snap ? snapValues : nullptr
						);


						if (ImGuizmo::IsUsing() && !Input::IsKeyPressed(Key::LeftAlt) && !Input::IsMouseButtonPressed(Mouse::Button1))
						{
							Entity parent = m_EditorScene->FindEntityByUUID(selectedEntity.GetParent());

							// If the entity has a parent, then the transform should be converted from local space to world space
							if (parent)
							{
								glm::mat4 parentTransform = m_EditorScene->GetTransformMatFromEntityAndParent(parent);
								transform = glm::inverse(parentTransform) * transform;
							}

							glm::vec3 translation, rotation, scale;
							Math::DecomposeTransform(transform, translation, rotation, scale);

							glm::vec3 deltaRotation = glm::degrees(rotation) - tc.Rotation;
							tc.Translation = translation;
							tc.Rotation += deltaRotation;
							tc.Scale = scale;
						}

					}
				}

				m_ViewportPanel->EndRender();
			});
			
			ImGui::Begin("Settings");
			UserInterface::Text("Camera Properties");
			UserInterface::SliderFloat("Exposure", m_EditorCamera->GetExposure(), 0.0f, 10.0f);
			UserInterface::SliderFloat("DOF", m_EditorCamera->GetDOF(), 0.0f, 5.0f);
			if (ImGui::Button("Play", { 60, 20 }))
			{
				m_SceneState = SceneState::Play;
			}
			if (ImGui::Button("Stop", { 60, 20 }))
			{
				m_SceneState = SceneState::Edit;
			}

			ImGui::End();

			ImGui::End();
		}

		void NewScene()
		{
			m_EditorScene = Ref<Scene>::Create();
			m_SceneHierarchyPanel->SetSceneContext(m_EditorScene);
		}

		void SaveSceneAs()
		{
			std::string filepath = FileDialogs::SaveFile("");

			if (!filepath.empty())
			{
				SceneSerializer sceneSerializer(m_EditorScene);
				sceneSerializer.Serialize(filepath);
			}
		}

		void OpenScene()
		{
			std::string filepath = FileDialogs::OpenFile("");

			if (!filepath.empty())
			{
				NewScene();
				SceneSerializer sceneSerializer(m_EditorScene);
				sceneSerializer.Deserialize(filepath);
			}
		}

		virtual void OnEvent(Event& event)
		{
			EventDispatcher dispatcher(event);
			dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event) { return OnKeyPressed(event); });

			m_EditorCamera->OnEvent(event);
			m_SceneHierarchyPanel->OnEvent(event);
			m_InspectorPanel->OnEvent(event);
		}

		bool OnKeyPressed(KeyPressedEvent& event)
		{
			if (event.GetRepeatCount() > 0) return false;

			bool leftControl = Input::IsKeyPressed(Key::LeftControl);

			switch (event.GetKeyCode())
			{
			// ImGuizmo modes
			case Key::Q:   if (!ImGuizmo::IsUsing()) m_GuizmoMode = -1; break;
			case Key::W:   if (!ImGuizmo::IsUsing()) m_GuizmoMode = ImGuizmo::OPERATION::TRANSLATE; break;
			case Key::E:   if (!ImGuizmo::IsUsing()) m_GuizmoMode = ImGuizmo::OPERATION::ROTATE; break;
			case Key::R:   if (!ImGuizmo::IsUsing()) m_GuizmoMode = ImGuizmo::OPERATION::SCALE; break;

			case Key::S: 
				{
					if (!leftControl) break;
					SaveSceneAs();
					break;
				}

			case Key::O: 
				{
					if (!leftControl) break;
					OpenScene();
					break;
				}
			}
			return false;
		}

		virtual void OnResize()
		{
		}

		virtual void OnDetach()
		{
			this->~EditorLayer();
		}
	private:
		// Editor Camera
		Ref<EditorCamera> m_EditorCamera;

		// Scenes
		Ref<Scene> m_EditorScene;

		// Panels
		Ref<SceneHierarchyPanel> m_SceneHierarchyPanel;
		Ref<InspectorPanel> m_InspectorPanel;
		Ref<ViewportPanel> m_ViewportPanel;
		Ref<MaterialEditor> m_MaterialEditor;
		Ref<AssetBrowser> m_AssetBrowser;

		// Variables
		int32_t m_GuizmoMode = -1;
		SceneState m_SceneState = SceneState::Edit;
	};
}