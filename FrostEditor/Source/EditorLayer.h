#pragma once

#include <Frost.h>

#include "Panels/SceneHierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/ViewportPanel.h"
#include "Panels/MaterialEditor.h"
#include "Panels/AssetBrowser.h"
#include "Panels/TitlebarPanel.h"

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

		virtual void OnEvent(Event& event);
		bool OnKeyPressed(KeyPressedEvent& event);

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
		Ref<TitlebarPanel> m_TitlebarPanel;
		Ref<AssetBrowser> m_AssetBrowser;

		// Variables
		int32_t m_GuizmoMode = -1;
		SceneState m_SceneState = SceneState::Edit;
	};
}