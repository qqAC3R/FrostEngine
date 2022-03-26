#pragma once

#include "Panel.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

namespace Frost
{
	class MaterialEditor : public Panel
	{
	public:
		MaterialEditor() = default;
		virtual ~MaterialEditor() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event & e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void SetActiveEntity(Entity& entity)
		{
			if (entity != m_SelectedEntity)
			{
				m_SelectedEntity = entity;
				m_SelectedMaterialIndex = UINT32_MAX;
			}
		}

	private:
		Entity m_SelectedEntity = {};
		uint32_t m_SelectedMaterialIndex = UINT32_MAX;
		bool m_Visibility = true;
	};
}