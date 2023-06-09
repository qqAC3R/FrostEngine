#pragma once

#include "Frost/EntitySystem/Entity.h"
#include "SceneHierarchyPanel.h"
#include "MaterialAssetEditor.h"
#include "PhysicsMaterialEditor.h"
#include "Panel.h"

namespace Frost
{
	class EditorLayer;

	class InspectorPanel : public Panel
	{
	public:
		InspectorPanel(EditorLayer* editorLayer);
		virtual ~InspectorPanel() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void SetHierarchy(Ref<SceneHierarchyPanel> scene)
		{
			FROST_ASSERT_INTERNAL(scene);
			m_SceneHierarchy = scene;
		}
		void SetMaterialAssetEditor(Ref<MaterialAssetEditor> materialAssetEditor)
		{
			FROST_ASSERT_INTERNAL(materialAssetEditor);
			m_MaterialAssetEditor = materialAssetEditor;
		}
		void SetPhysicsMaterialAssetEditor(Ref<MaterialAssetEditor> phyiscsMaterialEditor)
		{
			FROST_ASSERT_INTERNAL(phyiscsMaterialEditor);
			m_PhysicsMaterialEditor = phyiscsMaterialEditor;
		}
	private:
		void DrawComponents(Entity& entity);
	private:
		EditorLayer* m_Context = nullptr;
		Ref<SceneHierarchyPanel> m_SceneHierarchy = nullptr;
		Ref<MaterialAssetEditor> m_MaterialAssetEditor = nullptr;
		Ref<PhysicsMaterialEditor> m_PhysicsMaterialEditor = nullptr;
		std::string m_PanelNameImGui;

		bool m_Visibility = true;
	};

}