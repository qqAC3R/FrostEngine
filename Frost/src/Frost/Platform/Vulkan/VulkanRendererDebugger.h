#pragma once

#include "Frost/Renderer/Renderer.h"

namespace Frost
{
	class VulkanRendererDebugger : public RendererDebugger
	{
	public:
		VulkanRendererDebugger();
		virtual ~VulkanRendererDebugger();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void ImGuiRender() override;

	private:
		SceneRenderPassPipeline* m_SceneRenderPassPipeline;
	};
}