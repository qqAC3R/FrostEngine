#pragma once

#include "Panel.h"
#include "Frost/Physics/PhysicsMaterial.h"

namespace Frost
{
	class PhysicsMaterialEditor : public Panel
	{
	public:
		PhysicsMaterialEditor() = default;
		virtual ~PhysicsMaterialEditor() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void SetActiveMaterialAset(Ref<PhysicsMaterial> activePhysicsMaterial)
		{
			m_ActivePhysicsMaterial = activePhysicsMaterial;
		}

	private:
		bool m_Visibility = true;

		Ref<PhysicsMaterial> m_ActivePhysicsMaterial;
	};

}