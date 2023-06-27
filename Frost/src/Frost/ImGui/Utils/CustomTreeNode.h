#pragma once

#include "Frost/Renderer/Texture.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace ImGui
{
	bool TreeNodeWithIcon(Frost::Ref<Frost::Texture2D> icon, ImGuiID id, ImGuiTreeNodeFlags flags, const char* label, const char* label_end, ImColor iconTint = IM_COL32_WHITE);
}
