#pragma once

#include "Panel.h"
#include <glm/glm.hpp>

#include "Frost/Renderer/Texture.h"

namespace Frost
{
	class EditorLayer;
	enum class SceneState;

	class ViewportPanel : public Panel
	{
	public:
		ViewportPanel(EditorLayer* editorLayer);
		virtual ~ViewportPanel() = default;

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		void BeginRender();
		void RenderTexture(Ref<Image2D> texture);
		void EndRender();

		void RenderDebugTools(int32_t imguizmoMode);
		void RenderSceneButtons(SceneState& sceneState);
		void RenderViewportRenderPasses();

		bool IsResized() const { return m_IsResized; }
		glm::vec2 GetViewportPanelSize() const { return m_ViewportSize; }

		void SetScenePlayFunction(const std::function<void()>& func) { m_ScenePlayFunc = func; }
		void SetSceneStopFunction(const std::function<void()>& func) { m_SceneStopFunc = func; }
	private:
		void UpdateDragDrop();
	private:
		EditorLayer* m_Context = nullptr;

		glm::vec2 m_ViewportSize = glm::vec2(0.0f);
		bool m_Visibility = true;
		bool m_IsResized = false;
		bool m_IsResized_Internal = false;

		std::string m_OutputImageID = "FinalImage_With2D";

		std::function<void()> m_ScenePlayFunc, m_SceneStopFunc;

		friend class EditorLayer;
	};
}