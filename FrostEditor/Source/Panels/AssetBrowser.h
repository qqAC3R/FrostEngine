#pragma once

#include "Panel.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

namespace Frost
{
	class AssetBrowser : public Panel
	{
	public:
		AssetBrowser() = default;
		virtual ~AssetBrowser() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

	private:
		bool m_Visibility = true;
	};
}