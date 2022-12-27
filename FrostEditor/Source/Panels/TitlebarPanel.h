#pragma once

#include "Panel.h"
#include "Frost/Renderer/Texture.h"

namespace Frost
{

	class TitlebarPanel : public Panel
	{
	public:
		TitlebarPanel() = default;
		virtual ~TitlebarPanel() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		// TODO: I'm not sure if copying a std::function everyframe is a good option, but I'll change it later
		void SetMenuBar(const std::function<void()>& func) { m_MenuBarFunction = func; }
	private:
		void RenderTitleBar();

		void RenderButtons();
		void RenderMenuBar();
		void RenderEngineLogo();

		void HandleTitleBarDragZone();

	private:
		bool m_Visibility = true;

		// Textures
		Ref<Texture2D> m_LogoTexture;
		Ref<Texture2D> m_MinimizeButtonTex;
		Ref<Texture2D> m_MaximizeButtonTex;
		Ref<Texture2D> m_CloseButtonTex;

		// This is set manually by the editor layer
		std::function<void()> m_MenuBarFunction;

		const float m_TitlebarHeight = 57.0f;

		friend class InspectorPanel;
	};

}