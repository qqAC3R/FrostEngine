#pragma once

#include "Panel.h"
#include "Frost/Renderer/MaterialAsset.h"

namespace Frost
{
	class MaterialAssetEditor : public Panel
	{
	public:
		MaterialAssetEditor() = default;
		virtual ~MaterialAssetEditor() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void SetActiveMaterialAset(Ref<MaterialAsset> activeMaterialAsset)
		{
			m_ActiveMaterialAsset = activeMaterialAsset;
		}

	private:
		bool m_Visibility = true;

		Ref<MaterialAsset> m_ActiveMaterialAsset;
	};

}