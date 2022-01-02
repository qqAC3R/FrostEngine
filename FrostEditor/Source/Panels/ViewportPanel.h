#pragma once

#include "Panel.h"
#include <glm/glm.hpp>

#include "Frost/Renderer/Texture.h"

namespace Frost
{
	//class Texture2D;

	class ViewportPanel : public Panel
	{
	public:
		ViewportPanel() = default;
		virtual ~ViewportPanel() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void BeginRender();
		void RenderTexture(Ref<Texture2D> texture);
		void EndRender();

		bool IsResized() const { return m_IsResized; }
		glm::vec2 GetViewportPanelSize() const { return m_ViewportSize; }
	private:
		glm::vec2 m_ViewportSize = glm::vec2(0.0f);
		bool m_Visibility = true;
		bool m_IsResized = false;
	};
}