#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace Frost
{
	bool UpdateWindowManualResize(ImGuiWindow* window, ImVec2& newSize, ImVec2& newPosition);
	void RenderWindowOuterBorders(ImGuiWindow* window);
	void UIManualResize();
}