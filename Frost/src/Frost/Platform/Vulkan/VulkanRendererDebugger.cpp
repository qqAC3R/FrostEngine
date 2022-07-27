#include "frostpch.h"
#include "VulkanRendererDebugger.h"

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"

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

		const std::vector<float>& gpuTimings = VulkanRenderer::GetFrameExecutionTimings();

		ImGui::Begin("Performance");
		ImGui::Text("Total GPU Time: %.2f", gpuTimings[0]);
		ImGui::Separator();

		for (auto& timeStampPass : m_TimeStampPasses)
		{
			ImGui::Text("%s: %.2f", timeStampPass.first.c_str(), gpuTimings[timeStampPass.second / 2]);
		}
		ImGui::End();
	}

	void VulkanRendererDebugger::StartTimeStapForPass(const std::string& passName)
	{
		uint64_t queryID = VulkanRenderer::BeginTimestampQuery();
		m_TimeStampPasses[passName] = queryID;
	}

	void VulkanRendererDebugger::EndTimeStapForPass(const std::string& passName)
	{
		VulkanRenderer::EndTimestampQuery(m_TimeStampPasses[passName]);
	}

}