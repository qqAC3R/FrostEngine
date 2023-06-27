#pragma once

#include "Panel.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

#define HIERARCHY_ENTITY_DRAG_DROP "scene_entity_hierarchy"

namespace Frost
{
	
	class SceneHierarchyPanel : public Panel
	{
	public:
		SceneHierarchyPanel() = default;
		virtual ~SceneHierarchyPanel() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void SetSceneContext(Ref<Scene> scene)
		{
			FROST_ASSERT_INTERNAL(scene);
			m_SelectedEntity = {};
			m_SelectedSubmesh = UINT32_MAX;
			m_SceneContext = scene;
		}

		Entity& GetSelectedEntity() { return m_SelectedEntity; }
		uint32_t GetSelectedEntitySubmesh() { return m_SelectedSubmesh; }

		void SetSelectedEntityByID(uint32_t entityID);
		void SetSelectedEntity(Entity ent)
		{
			m_SelectedEntity = ent;
			m_SelectedSubmesh = UINT32_MAX;
		}
	private:
		void DrawEntityNode(Entity& entity);

		void AddLateFunction(const std::function<void()>& func);
		void ExecuteLateFunctions();
	private:
		Ref<Scene> m_SceneContext = nullptr;
		Entity m_PreviousEntity = {};
		Entity m_SelectedEntity = {};

		Vector<std::function<void()>> m_LateDeletionFunctions;

		// This is optional, and only works when the selected entity has a mesh component
		uint32_t m_SelectedSubmesh = UINT32_MAX;

		bool m_Visibility = true;

		friend class InspectorPanel;
	};

}