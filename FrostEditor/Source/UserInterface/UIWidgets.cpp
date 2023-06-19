#include "frostpch.h"
#include "UIWidgets.h"

#include "Frost/Utils/PlatformUtils.h"	

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "Frost/EntitySystem/Entity.h"
#include "Frost/EntitySystem/Components.h"
#include "Panels/SceneHierarchyPanel.h"

namespace Frost
{
	static bool IsItemDisabled()
	{
		return ImGui::GetItemFlags() & ImGuiItemFlags_Disabled;
	}

	void UserInterface::DrawVec3ColorEdit(const std::string& name, glm::vec3& values)
	{
		ImGui::ColorEdit3(name.c_str(), reinterpret_cast<float*>(&values));
	}

	void UserInterface::DrawVec4ColorEdit(const std::string& name, glm::vec4& values)
	{
		ImGui::ColorEdit4(name.c_str(), reinterpret_cast<float*>(&values));
	}

	void UserInterface::DrawVec3CoordsEdit(const std::string& name, glm::vec3& values, float resetValue /*= 0.0f*/, float columnWidth /*= 100.0f*/)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0); // Rouding for the buttons 

		ImGui::PushID(name.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(name.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0.5f });

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		//bool ImGui::DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();

		ImGui::PopStyleVar(); // Rouding for the buttons 
	}

	std::string UserInterface::DrawFilePath(const std::string& label, const std::string& textBox, const std::string& format, float columnWidth)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };


		ImGui::PushID(label.c_str());

		ImGui::Columns(3);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::SetColumnWidth(1, ImGui::GetWindowWidth() - columnWidth - buttonSize.x - 45.0f);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		std::strncpy(buffer, textBox.c_str(), sizeof(buffer));
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##Tag", buffer, sizeof(buffer));

		ImGui::NextColumn();

		//float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
		//ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
		std::string path;
		if (ImGui::Button("..", buttonSize))
		{
			path = FileDialogs::OpenFile("");
		}

		ImGui::Columns(1);
		ImGui::PopID();

		return path;
	}

	void UserInterface::CheckBox(const std::string& label, bool& value)
	{
		ImGui::Checkbox(label.c_str(), &value);
	}

	bool UserInterface::InputText(const std::string& label, const char* buffer)
	{
		//char buffer[256];
		//memset(buffer, 0, sizeof(buffer));
		//std::strncpy(buffer, component.ModuleName.c_str(), sizeof(buffer));
		//ImGui::SetNextItemWidth(-1.0f);
		//bool action = ImGui::InputText("##Tag", buffer, sizeof(buffer));
		return false;
	}

	bool UserInterface::PropertyPrefabReference(const char* label, const char* prefabName)
	{
		bool isButtonClicked = false;

		{
			float width = ImGui::GetContentRegionAvail().x;
			float itemHeight = 23.0f;

			
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(3, 161, 252, 255)); // Prefab blue color
			if (ImGui::Button(prefabName, { width, itemHeight }))
				isButtonClicked = true;
			ImGui::PopStyleColor();
				
		}
		return isButtonClicked;


		// TODO: This should work with the content browser
#if 0
		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload(HIERARCHY_ENTITY_DRAG_DROP);
			if (data)
			{
				entity = *(Entity*)data->Data;
				receivedValidEntity = true;
			}

			ImGui::EndDragDropTarget();
		}
#endif


		//return receivedValidEntity;
	}

	bool UserInterface::PropertyEntityReference(const char* label, Entity& entity)
	{
		bool receivedValidEntity = false;

		//ImVec2 originalButtonTextAlign = ImGui::GetStyle().ButtonTextAlign;
		{
			//ImGui::GetStyle().ButtonTextAlign = { 0.0f, 0.5f };
			float width = ImGui::GetContentRegionAvail().x;
			float itemHeight = 23.0f;

			std::string buttonText = "Null";

			if (entity)
				buttonText = entity.GetComponent<TagComponent>().Tag;

			ImGui::Button(buttonText.c_str(), {width, itemHeight});
		}
		//ImGui::GetStyle().ButtonTextAlign = originalButtonTextAlign;

		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload(HIERARCHY_ENTITY_DRAG_DROP);
			if (data)
			{
				entity = *(Entity*)data->Data;
				receivedValidEntity = true;
			}

			ImGui::EndDragDropTarget();
		}

		//ImGui::PopItemWidth();
		//ImGui::NextColumn();

		return receivedValidEntity;
	}

	static char* s_MultilineBuffer = nullptr;

	bool UserInterface::PropertyMultiline(const char* label, std::string& value)
	{
		bool modified = false;

		ImGui::TableNextColumn();
		ImGui::Text(label);

		if (!s_MultilineBuffer)
		{
			s_MultilineBuffer = new char[1024 * 1024]; // 1KB
			memset(s_MultilineBuffer, 0, 1024 * 1024);
		}

		strcpy(s_MultilineBuffer, value.c_str());

		ImGui::TableNextColumn();
		ImGui::PushItemWidth(-1);
		if (ImGui::InputTextMultiline("", s_MultilineBuffer, 1024 * 1024))
		{
			value = s_MultilineBuffer;
			modified = true;
		}
		ImGui::PopItemWidth();

		return modified;
	}

	void UserInterface::Text(const std::string& text)
	{
		ImGui::Text(text.c_str());
	}

	void UserInterface::SliderFloat(const std::string& name, float& value, float min, float max)
	{
		ImGui::SliderFloat(name.c_str(), reinterpret_cast<float*>(&value), min, max);
	}

	void UserInterface::DragFloat(const std::string& name, float& value, float speed, float minValue, float maxValue)
	{
		ImGui::DragFloat(name.c_str(), &value, speed, minValue, maxValue);
	}
}