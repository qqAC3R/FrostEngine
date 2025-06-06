#include "frostpch.h"
#include "ViewportPanel.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Core/Application.h"
#include "Frost/Core/Input.h"
#include "Frost/InputCodes/MouseButtonCodes.h"
#include "Frost/EntitySystem/Prefab.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/ImGui/ImGuiLayer.h"
#include "Frost/ImGui/Utils/ScopedStyle.h"

#include "IconsFontAwesome.hpp"

#include "EditorLayer.h"
#include "Panels/ContentBrowser/ContentBrowser.h"
#include "Panels/ContentBrowser/ContentBrowserSelectionStack.h"
#include "UserInterface/UIWidgets.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Frost
{
	//  TODO: This is also called in "EditorLayer.h",
	// but currently including the header gives linking errors, fix that!
	//enum class SceneState
	//{
	//	Edit,
	//	Play
	//};

	ViewportPanel::ViewportPanel(EditorLayer* editorLayer)
		: m_Context(editorLayer) {}

	void ViewportPanel::Init(void* initArgs)
	{
	}

	void ViewportPanel::OnEvent(Event& e)
	{

	}

	void ViewportPanel::Render()
	{
	}

	void ViewportPanel::BeginRender()
	{
		// Begin the viewport panel
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoCollapse);

		//ImGui::BeginChild("ViewportPanel");

		// Check if the viewport is focused
		bool IsViewPortFocused = ImGui::IsWindowFocused();
		Application::Get().GetImGuiLayer()->BlockEvents(!IsViewPortFocused);

		// Check if the viewport is resized
		ImVec2 viewPortSizePanel = ImGui::GetContentRegionAvail();
		if (m_ViewportSize.x != viewPortSizePanel.x || m_ViewportSize.y != viewPortSizePanel.y)
		{
			//m_IsResized = true;

			//m_IsResized = true;
			m_IsResized_Internal = true;

			m_ViewportSize = *(glm::vec2*)&viewPortSizePanel;
		}
		else
		{

			//m_IsResized = false;
#if 1
			if(m_IsResized_Internal && !Input::IsMouseButtonPressed(Mouse::Button0))
			{
				m_IsResized = true;
				m_IsResized_Internal = false;
			}
			else
			{
				m_IsResized = false;
			}
#endif

		}

	}

	void ViewportPanel::RenderTexture(Ref<Image2D> texture)
	{
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		imguiLayer->RenderTexture(texture, m_ViewportSize.x, m_ViewportSize.y);
	}

	void ViewportPanel::UpdateDragDrop()
	{
		if (ImGui::BeginDragDropTarget())
		{
			ImGui::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

			auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
			if (data)
			{
				SelectionData selectionData = *(SelectionData*)data->Data;

				if (selectionData.AssetType == AssetType::MeshAsset)
				{
					Entity ent = m_Context->m_CurrentScene->CreateEntity("New Mesh");
					Ref<Mesh> mesh = Ref<Mesh>::Create(AssetManager::GetOrLoadAsset<MeshAsset>(selectionData.FilePath.string()));

					MeshComponent& meshComponent = ent.AddComponent<MeshComponent>();
					meshComponent.Mesh = mesh;

					m_Context->m_SceneHierarchyPanel->SetSelectedEntity(ent);
				}

				if (selectionData.AssetType == AssetType::Scene)
				{
					m_Context->OpenSceneWithFilepath(selectionData.FilePath.string());
				}

				if (selectionData.AssetType == AssetType::Prefab)
				{
					Ref<Prefab> prefab = AssetManager::GetOrLoadAsset<Prefab>(selectionData.FilePath.string());
					if (prefab)
					{
						Entity ent = m_Context->m_CurrentScene->Instantiate(prefab);
						m_Context->m_SceneHierarchyPanel->SetSelectedEntity(ent);
					}
				}

			}
		}
	}

	void ViewportPanel::EndRender()
	{
		// End the viewport panel rendering


		{
			// Offset a bit to see the outlined border of the drag drop
			ImGui::SetCursorPos({ 5, 5 });

			ImGui::ScopedStyle buttonColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::ScopedStyle buttonColorActive(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::ScopedStyle buttonColorHovered(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::ScopedStyle borderColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

			// Same here, offset the button with 5 pixels (in both sides) to see the outlined bordered of the drag drop
			// Also a custom function was required for InvisibleButton, because this should work as a dummy and should not add any events (only for rendering)
			UserInterface::InvisibleButtonWithNoBehaivour("##InvisibleButtonDragDrop", { m_ViewportSize.x - 10.0f, m_ViewportSize.y - 10.0f });

			UpdateDragDrop();
		}
		

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::RenderDebugTools(int32_t imguizmoMode)
	{
		// ===============================================================================
		{
			// ImGuizmo tool picker

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

			// Mouse
			ImGui::SetCursorPosX(5.0f + 5.0f);
			ImGui::SetCursorPosY(7.0f);

			if (imguizmoMode == -1) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.44f, 0.0f, 1.0f));
			if (ImGui::Button(ICON_MOUSE_POINTER, { 30, 30 }))
			{
				if (imguizmoMode == -1)
					ImGui::PopStyleColor();
				imguizmoMode = -1;
			}
			else
			{
				if (imguizmoMode == -1)
					ImGui::PopStyleColor();
			}

			// Move
			ImGui::SetCursorPosX(35.0f + 5.0f);
			ImGui::SetCursorPosY(7.0f);
			if (imguizmoMode == ImGuizmo::OPERATION::TRANSLATE) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.44f, 0.0f, 1.0f));
			if (ImGui::Button(ICON_ARROWS, { 30, 30 }))
			{
				if (imguizmoMode == ImGuizmo::OPERATION::TRANSLATE)
					ImGui::PopStyleColor();
				imguizmoMode = ImGuizmo::OPERATION::TRANSLATE;
			}
			else
			{
				if (imguizmoMode == ImGuizmo::OPERATION::TRANSLATE)
					ImGui::PopStyleColor();
			}

			// Rotate
			ImGui::SetCursorPosX(65.0f + 5.0f);
			ImGui::SetCursorPosY(7.0f);
			if (imguizmoMode == ImGuizmo::OPERATION::ROTATE) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.44f, 0.0f, 1.0f));
			if (ImGui::Button(ICON_UNDO, { 30, 30 }))
			{
				if (imguizmoMode == ImGuizmo::OPERATION::ROTATE)
					ImGui::PopStyleColor();
				imguizmoMode = ImGuizmo::OPERATION::ROTATE;
			}
			else
			{
				if (imguizmoMode == ImGuizmo::OPERATION::ROTATE)
					ImGui::PopStyleColor();
			}

			// Scale
			ImGui::SetCursorPosX(95.0f + 5.0f);
			ImGui::SetCursorPosY(7.0f);
			if (imguizmoMode == ImGuizmo::OPERATION::SCALE) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.44f, 0.0f, 1.0f));
			if (ImGui::Button(ICON_EXPAND, { 30, 30 }))
			{
				if (imguizmoMode == ImGuizmo::OPERATION::SCALE)
					ImGui::PopStyleColor();
				imguizmoMode = ImGuizmo::OPERATION::SCALE;
			}
			else
			{
				if (imguizmoMode == ImGuizmo::OPERATION::SCALE)
					ImGui::PopStyleColor();
			}

			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();


			// --------------------------------
			ImGui::SetCursorPosX(7.0f);
			ImGui::SetCursorPosY(7.0f);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

			ImGui::Button("", { 123, 30 });

			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
		}
	}

	void ViewportPanel::RenderSceneButtons(SceneState& sceneState)
	{
		// ===============================================================================
		{
			// Play/Pause Buttons

			ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - 40.0f);
			ImGui::SetCursorPosY(7.0f);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

			if (sceneState == SceneState::Edit)
			{
				if (ImGui::Button(ICON_PLAY, { 40, 30 }))
				{
					m_ScenePlayFunc();
				}
			}
			else
			{
				if (ImGui::Button(ICON_STOP, { 40, 30 }))
				{
					m_SceneStopFunc();
				}
			}

			ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2));
			ImGui::SetCursorPosY(7.0f);
			ImGui::Button(ICON_PAUSE, { 40, 30 }); // TODO: Pause scene!

			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();


			ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2) - 45.0f);
			ImGui::SetCursorPosY(7.0f);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

			ImGui::Button("", { 90, 30 });
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
		}
	}

	void ViewportPanel::RenderViewportRenderPasses()
	{
		// ===============================================================================
		{
			// Scene Debug Buttons

			ImGui::SetCursorPosX((ImGui::GetWindowWidth()) - 45.0f);
			ImGui::SetCursorPosY(7.0f);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);


			bool callPopup = false;
			if (ImGui::Button(ICON_COG, { 40, 30 }))
			{
				callPopup = true;
			}

			if (callPopup)
			{
				ImGui::OpenPopup("Debug Viewer", ImGuiPopupFlags_AnyPopup);
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 5.0f, 5.0f });

			if (ImGui::BeginPopup("Debug Viewer"))
			{
				for (auto& output : Renderer::GetOutputImageMap())
				{
					if (ImGui::MenuItem(output.first.c_str()))
					{
						m_OutputImageID = output.first;
					}
				}
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();


			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();


			ImGui::SetCursorPosX((ImGui::GetWindowWidth()) - 45.0f);
			ImGui::SetCursorPosY(7.0f);

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.09f, 0.09f, 0.09f, 0.3f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

			ImGui::Button("", { 40, 30 });
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();
		}
	}

	void ViewportPanel::Shutdown()
	{
	}
}