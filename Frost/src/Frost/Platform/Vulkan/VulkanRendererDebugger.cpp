#include "frostpch.h"
#include "VulkanRendererDebugger.h"

#include "Frost/Renderer/SceneRenderPass.h"

#include <imgui.h>

namespace Frost
{
	VulkanRendererDebugger::VulkanRendererDebugger()
	{

	}

	VulkanRendererDebugger::~VulkanRendererDebugger()
	{

	}

	void VulkanRendererDebugger::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_SceneRenderPassPipeline = renderPassPipeline;
	}

	void VulkanRendererDebugger::ImGuiRender()
	{
		ImGui::Begin("Renderer Debugger");
		m_SceneRenderPassPipeline->UpdateRendererDebugger();
		ImGui::End();
	}
}