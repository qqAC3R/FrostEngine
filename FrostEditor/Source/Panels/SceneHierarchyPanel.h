#pragma once

#include "Panel.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

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
			m_SceneContext = scene;
		}

		Entity& GetSelectedEntity() { return m_SelectedEntity; }

	private:
		void DrawEntityNode(Entity& entity);
	private:
		Ref<Scene> m_SceneContext = nullptr;
		Entity m_SelectedEntity = {};

		bool m_Visibility = true;
	};

}