#include "frostpch.h"
#include "EditorLayer.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include "Frost/Core/Input.h"
#include "Frost/Events/KeyEvent.h"
#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/InputCodes/MouseButtonCodes.h"
#include "Frost/Physics/PhysicsEngine.h"

#include "Frost/Math/Math.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "UserInterface/UIWidgets.h"

#include "IconsFontAwesome.hpp"

namespace Frost
{

	EditorLayer::EditorLayer()
		: Layer("Editor")
	{
	}

	EditorLayer::~EditorLayer()
	{
	}

	void EditorLayer::OnAttach()
	{
		Application::Get().GetWindow().SetWindowProjectName("Untilted scene");

		m_EditorCamera = Ref<EditorCamera>::Create(85.0, 1600.0f / 900.0f, 0.1f, 2000.0f);
	
		// Scene initialization
		m_EditorScene = Ref<Scene>::Create();
		m_CurrentScene = m_EditorScene;

		// Scene Hierarchy Panel initialization
		m_SceneHierarchyPanel = Ref<SceneHierarchyPanel>::Create();
		m_SceneHierarchyPanel->Init(nullptr);
		m_SceneHierarchyPanel->SetSceneContext(m_CurrentScene);

		// Inspector Panel initialization
		m_InspectorPanel = Ref<InspectorPanel>::Create();
		m_InspectorPanel->Init(nullptr);
		m_InspectorPanel->SetHierarchy(m_SceneHierarchyPanel);

		m_ViewportPanel = Ref<ViewportPanel>::Create();
		m_ViewportPanel->Init(nullptr);
		m_ViewportPanel->SetScenePlayFunction(std::bind(&EditorLayer::OnScenePlay, this));
		m_ViewportPanel->SetSceneStopFunction(std::bind(&EditorLayer::OnSceneStop, this));

		m_MaterialEditor = Ref<MaterialEditor>::Create();
		m_MaterialEditor->Init(nullptr);

		m_AssetBrowser = Ref<AssetBrowser>::Create();
		m_AssetBrowser->Init(nullptr);

		m_TitlebarPanel = Ref<TitlebarPanel>::Create();
		m_TitlebarPanel->Init(nullptr);


		{
			auto& sponzaEntity = m_CurrentScene->CreateEntity("Plane");
			auto& meshComponent = sponzaEntity.AddComponent<MeshComponent>();
			meshComponent.Mesh = Mesh::Load("Resources/Meshes/Plane.obj", { glm::vec3(1.0f), glm::vec3(1.0f), 0.0f, 1.0f });
		}

		{
			auto& directionalLight = m_CurrentScene->CreateEntity("Directional Light");
			auto& directionalLightComponent = directionalLight.AddComponent<DirectionalLightComponent>();
			TransformComponent& ts = directionalLight.GetComponent<TransformComponent>();
			ts.Translation = { 0.0f, 1.0f, 0.0f };
			ts.Rotation = { -90.0f, 0.0f, 0.0f };
		}
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		if (m_SceneState == SceneState::Play)
		{
			CameraComponent* primaryCamera = m_CurrentScene->GetPrimaryCamera();

			if (primaryCamera)
			{
				if (m_SceneState == SceneState::Play)
				{
					primaryCamera->Camera->SetViewportSize(m_ViewportPanel->GetViewportPanelSize().x, m_ViewportPanel->GetViewportPanelSize().y);
					Renderer::BeginScene(primaryCamera->Camera);

					m_CurrentScene->UpdateRuntime(ts);
				}
			}
			else
			{
				m_EditorCamera->OnUpdate(ts);
				Renderer::BeginScene(m_EditorCamera);
				m_CurrentScene->Update(ts);

				FROST_CORE_WARN("The scene doesn't have a camera!");
			}
		}
		else
		{
			m_EditorCamera->OnUpdate(ts);
			Renderer::BeginScene(m_EditorCamera);
			m_CurrentScene->Update(ts);
		}

		
		Renderer::EndScene();
	}

	void EditorLayer::OnImGuiRender()
	{
		Renderer::Submit([&]()
		{
			static bool dockSpaceOpen = true;
			static bool opt_fullscreen = true;

			static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

			ImGuiWindowFlags window_flags = /*ImGuiWindowFlags_MenuBar |*/ ImGuiWindowFlags_NoDocking;
			if (opt_fullscreen)
			{
				ImGuiViewport* viewport = ImGui::GetMainViewport();

				ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y));
				ImGui::SetNextWindowSize(viewport->WorkSize);
				ImGui::SetNextWindowViewport(viewport->ID);
				ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{ 0.0f, 0.0f, 0.0f, 0.0f });
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
				window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
				window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			}


			ImGui::Begin("DockSpace", nullptr, window_flags);


			m_TitlebarPanel->SetMenuBar([this]()
			{
				if (ImGui::Button("File"))
					ImGui::OpenPopup("FilePopup");
				if (ImGui::BeginPopup("FilePopup"))
				{
					if (ImGui::MenuItem("Open"))
						OpenScene();
					if (ImGui::MenuItem("Save As"))
						SaveSceneAs();

					ImGui::Separator();

					if (ImGui::MenuItem("Exit"))
						Application::Get().Close();

					ImGui::EndPopup();
				}

				ImGui::SameLine();

				if (ImGui::Button("Debug"))
					ImGui::OpenPopup("DebugPopup");
				if (ImGui::BeginPopup("DebugPopup"))
				{
					if (ImGui::MenuItem("Physics Debug", "", &m_EnablePhysicsDebugging))
						PhysicsEngine::EnableDebugRecording(m_EnablePhysicsDebugging);

					ImGui::EndPopup();
				}
			});
			m_TitlebarPanel->Render();


			if (opt_fullscreen)
			{
				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor();
			}

			// DockSpace
			ImGuiIO& io = ImGui::GetIO();
			ImGuiStyle& style = ImGui::GetStyle();

			float minWinSizeX = style.WindowMinSize.x;
			style.WindowMinSize.x = 370.0f;
			if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
			{
				ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
				ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
				ImGui::End();
			}
			style.WindowMinSize.x = minWinSizeX;


			ImGui::Begin("Settings");
			UserInterface::Text("Camera Properties");
			UserInterface::SliderFloat("Exposure", m_EditorCamera->GetExposure(), 0.0f, 10.0f);
			UserInterface::SliderFloat("DOF", m_EditorCamera->GetDOF(), 0.0f, 5.0f);
			ImGui::End();





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
			}



			Ref<Image2D> texture = Renderer::GetFinalImage(m_ViewportPanel->m_OutputImageID);
			//Ref<Image2D> texture = Renderer::GetFinalImage(1);
			m_ViewportPanel->RenderTexture(texture);


			m_ViewportPanel->RenderDebugTools(m_GuizmoMode);
			m_ViewportPanel->RenderSceneButtons(m_SceneState);
			m_ViewportPanel->RenderViewportRenderPasses();

			if (m_SceneState != SceneState::Play)
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
	}

	void EditorLayer::NewScene()
	{
		if (m_SceneState == SceneState::Play) return;

		m_EditorScene = Ref<Scene>::Create();
		m_SceneHierarchyPanel->SetSceneContext(m_EditorScene);
		SetCurrentScene(m_EditorScene);
		Application::Get().GetWindow().SetWindowProjectName("Untilted scene");
	}

	void EditorLayer::SaveSceneAs()
	{
		if (m_SceneState == SceneState::Play) return;

		std::string filepath = FileDialogs::SaveFile("");

		if (!filepath.empty())
		{
			SceneSerializer sceneSerializer(m_EditorScene);
			sceneSerializer.Serialize(filepath);
			Application::Get().GetWindow().SetWindowProjectName(sceneSerializer.GetSceneName());
		}
	}

	void EditorLayer::OpenScene()
	{
		if (m_SceneState == SceneState::Play) return;

		std::string filepath = FileDialogs::OpenFile("");

		if (!filepath.empty())
		{
			NewScene();
			SceneSerializer sceneSerializer(m_CurrentScene);
			sceneSerializer.Deserialize(filepath);
			Application::Get().GetWindow().SetWindowProjectName(sceneSerializer.GetSceneName());

			m_EditorScene = m_CurrentScene;
			m_SceneHierarchyPanel->SetSceneContext(m_EditorScene);
		}
	}

	void EditorLayer::OnScenePlay()
	{
		m_SceneState = SceneState::Play;

		m_RuntimeScene = Ref<Scene>::Create();
		m_EditorScene->CopyTo(m_RuntimeScene);
		m_SceneHierarchyPanel->SetSceneContext(m_RuntimeScene);

		m_RuntimeScene->OnPhysicsSimulationStart();

		SetCurrentScene(m_RuntimeScene);
	}

	void EditorLayer::OnSceneStop()
	{
		m_SceneState = SceneState::Edit;
		m_SceneHierarchyPanel->SetSceneContext(m_EditorScene);

		m_RuntimeScene->OnPhysicsSimulationEnd();

		m_RuntimeScene = nullptr;

		SetCurrentScene(m_EditorScene);
	}

	void EditorLayer::OnEvent(Event& event)
	{
		EventDispatcher dispatcher(event);
		dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event) { return OnKeyPressed(event); });

		m_EditorCamera->OnEvent(event);
		m_SceneHierarchyPanel->OnEvent(event);
		m_InspectorPanel->OnEvent(event);
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& event)
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

		case Key::N:
		{
			if (!leftControl) break;
			NewScene();
			break;
		}

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

	void EditorLayer::OnResize()
	{
	}

	void EditorLayer::OnDetach()
	{
		this->~EditorLayer();
	}

}