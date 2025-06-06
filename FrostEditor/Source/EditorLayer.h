#pragma once

#include <Frost.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Panels/MaterialEditor.h"
#include "Panels/ContentBrowser/ContentBrowser.h"
#include "Panels/TitlebarPanel.h"
#include "Panels/MaterialAssetEditor.h"
#include "Panels/PhysicsMaterialEditor.h"
#include "Panels/AnimationNodeEditor/AnimationNodeEditor.h"

namespace Frost
{
	// GImGui->FontSize
	// Window Padding - table
	// Frame padding - entities

	enum class SceneState
	{
		Edit,
		Play
	};

	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		~EditorLayer();

		virtual void OnAttach();
		virtual void OnUpdate(Timestep ts);
		virtual void OnImGuiRender();

		void NewScene();
		void SaveSceneAs();
		void OpenScene();
		void OpenSceneWithFilepath(const std::string& filepath);

		virtual void OnEvent(Event& event);
		bool OnKeyPressed(KeyPressedEvent& event);
		//bool OnMouseButtonPressed(MouseButtonPressedEvent& event);
		void OnMouseClickSelectEntity();

		virtual void OnResize();

		virtual void OnDetach();
	private:
		void OnScenePlay();
		void OnSceneStop();

		void SetCurrentScene(Ref<Scene> scene) { m_CurrentScene = scene; }
		std::pair<uint32_t, uint32_t> GetMouseViewportSpace();
	private:
		// Editor Camera
		Ref<EditorCamera> m_EditorCamera;

		// Scenes
		Ref<Scene> m_EditorScene, m_RuntimeScene, m_CurrentScene;

		// Panels
		Ref<SceneHierarchyPanel> m_SceneHierarchyPanel;
		Ref<InspectorPanel> m_InspectorPanel;
		Ref<ViewportPanel> m_ViewportPanel;
		//Ref<MaterialEditor> m_MaterialEditor;
		Ref<TitlebarPanel> m_TitlebarPanel;
		Ref<ContentBrowserPanel> m_ContentBrowser;
		Ref<MaterialAssetEditor> m_MaterialAssetEditor;
		Ref<PhysicsMaterialEditor> m_PhysicsMaterialEditor;
		Ref<AnimationNoteEditor> m_AnimationNodeEditor;

		bool m_ShowSceneHierarchyPanel = true;
		bool m_ShowInspectorPanel = true;
		bool m_ShowContentBrowser = true;
		bool m_ShowRendererDebugger = true;

		bool m_ViewPortMouseHovered = false;
		bool m_IsViewPortFocused = false;

		// Variables
		int32_t m_GuizmoMode = -1;
		SceneState m_SceneState = SceneState::Edit;
		bool m_EnablePhysicsDebugging = false;
		bool m_EnablePhysicsVisualization = true;
		bool m_IsGuizmoUsing = false;
		bool m_WasMousePressedPrevFrame = false;

		glm::vec2 m_ViewportSize;
		glm::vec2 m_ViewportBounds[2];

		friend class InspectorPanel;
		friend class ViewportPanel;
	};
}