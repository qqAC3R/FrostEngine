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

		virtual void StartTimeStapForPass(const std::string& passName) override;
		virtual void EndTimeStapForPass(const std::string& passName) override;

	private:
		SceneRenderPassPipeline* m_SceneRenderPassPipeline;

		using QueryID = uint64_t;
		HashMap<std::string, QueryID> m_TimeStampPasses;
	};
}