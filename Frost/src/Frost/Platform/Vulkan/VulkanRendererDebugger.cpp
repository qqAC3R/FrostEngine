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

	void VulkanRendererDebugger::ImGuiRender()
	{
		ImGui::Begin("Renderer Debugger");



		//bool* lightHeatMap = 
		//ImGui::Checkbox("Light Heat Map", )
		ImGui::End();
	}
}