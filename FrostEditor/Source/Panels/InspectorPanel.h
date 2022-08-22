#pragma once

#include "Frost/EntitySystem/Entity.h"
#include "SceneHierarchyPanel.h"
#include "Panel.h"

namespace Frost
{

	class InspectorPanel : public Panel
	{
	public:
		InspectorPanel();
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
	private:
		void DrawComponents(Entity& entity);
	private:
		Ref<SceneHierarchyPanel> m_SceneHierarchy = nullptr;
		std::string m_PanelNameImGui;

		bool m_Visibility = true;
	};

}