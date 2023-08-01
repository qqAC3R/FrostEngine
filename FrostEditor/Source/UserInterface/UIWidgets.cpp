#include "frostpch.h"
#include "UIWidgets.h"

#include "Frost/Utils/PlatformUtils.h"	

#include "Frost/EntitySystem/Entity.h"
#include "Frost/EntitySystem/Components.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowser/ContentBrowser.h"
#include "Panels/ContentBrowser/ContentBrowserSelectionStack.h"

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
		std::string filePath;

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

		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
			if (data)
			{
				SelectionData selectionData = *(SelectionData*)data->Data;
				
				if (selectionData.AssetType == AssetType::MeshAsset)
					filePath = selectionData.FilePath.string();
				else
					FROST_CORE_WARN("Dragged-Dropped wrong asset type (Need Mesh Asset!)");
			}

			ImGui::EndDragDropTarget();
		}

		ImGui::NextColumn();

		
		if (ImGui::Button("..", buttonSize))
		{
			filePath = FileDialogs::OpenFile("");
		}

		ImGui::Columns(1);
		ImGui::PopID();

		return filePath;
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

		{
			float width = ImGui::GetContentRegionAvail().x;
			float itemHeight = 23.0f;

			std::string buttonText = "Null";

			if (entity)
				buttonText = entity.GetComponent<TagComponent>().Tag;

			ImGui::Button(buttonText.c_str(), {width, itemHeight});
		}

		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
			if (data)
			{
				ContentBrowserDragDropData dragDropData = *(ContentBrowserDragDropData*)data->Data;

				if (dragDropData.AssetType == AssetType::Prefab)
				{
					entity = *(Entity*)dragDropData.Data;
					receivedValidEntity = true;
				}
			}

			ImGui::EndDragDropTarget();
		}

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

	void UserInterface::InvisibleButtonWithNoBehaivour(const char* str_id, const ImVec2& size_arg)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;

		// Cannot use zero-size for InvisibleButton(). Unlike Button() there is not way to fallback using the label size.
		IM_ASSERT(size_arg.x != 0.0f && size_arg.y != 0.0f);

		const ImGuiID id = window->GetID(str_id);
		ImVec2 size = ImGui::CalcItemSize(size_arg, 0.0f, 0.0f);
		const ImRect bb(window->DC.CursorPos, { window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y });
		ImGui::ItemSize(size);
		if (!ImGui::ItemAdd(bb, id))
			return;
	}

	void UserInterface::ShiftCursorX(float x)
	{
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);
	}

	void UserInterface::ShiftCursorY(float y)
	{
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + y);
	}

	void UserInterface::DrawButtonImage(Ref<Texture2D> imageNormal, Ref<Texture2D> imageHovered, Ref<Texture2D> imagePressed, ImU32 tintNormal, ImU32 tintHovered, ImU32 tintPressed, ImVec2 rectMin, ImVec2 rectMax)
	{
		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		auto* drawList = ImGui::GetWindowDrawList();
		if (ImGui::IsItemActive())
			drawList->AddImage((ImTextureID)imguiLayer->GetImGuiTextureID(imagePressed->GetImage2D()), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintPressed);
		else if (ImGui::IsItemHovered())
			drawList->AddImage((ImTextureID)imguiLayer->GetImGuiTextureID(imageHovered->GetImage2D()), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintHovered);
		else
			drawList->AddImage((ImTextureID)imguiLayer->GetImGuiTextureID(imageNormal->GetImage2D()), rectMin, rectMax, ImVec2(0, 0), ImVec2(1, 1), tintNormal);
	}

	void UserInterface::DrawShadowInner(const Ref<Texture2D>& shadowImage, int radius, ImVec2 rectMin, ImVec2 rectMax, float alpha /*= 1.0f*/, float lengthStretch /*= 10.0f*/, bool drawLeft /*= true*/, bool drawRight /*= true*/, bool drawTop /*= true*/, bool drawBottom /*= true*/)
	{
		const float widthOffset = lengthStretch;
		const float alphaTop = alpha; //std::min(0.25f * alphMultiplier, 1.0f);
		const float alphaSides = alpha; //std::min(0.30f * alphMultiplier, 1.0f);
		const float alphaBottom = alpha; //std::min(0.60f * alphMultiplier, 1.0f);
		const auto p1 = ImVec2(rectMin.x + radius, rectMin.y + radius);
		const auto p2 = ImVec2(rectMax.x - radius, rectMax.y - radius);
		auto* drawList = ImGui::GetWindowDrawList();

		ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
		ImTextureID textureID = (ImTextureID)imguiLayer->GetImGuiTextureID(shadowImage->GetImage2D());

		drawList->PushClipRect({ p2.x + 1.0f, p1.y - widthOffset }, { p2.x + radius, p2.y + widthOffset });

		if (drawTop)
			drawList->AddImage(textureID, { p1.x - widthOffset,  p1.y - radius }, { p2.x + widthOffset, p1.y }, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f), ImColor(0.0f, 0.0f, 0.0f, alphaTop));
		if (drawBottom)
			drawList->AddImage(textureID, { p1.x - widthOffset,  p2.y }, { p2.x + widthOffset, p2.y + radius }, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), ImColor(0.0f, 0.0f, 0.0f, alphaBottom));
		if (drawLeft)
			drawList->AddImageQuad(textureID, { p1.x - radius, p1.y - widthOffset }, { p1.x, p1.y - widthOffset }, { p1.x, p2.y + widthOffset }, { p1.x - radius, p2.y + widthOffset },
				{ 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, ImColor(0.0f, 0.0f, 0.0f, alphaSides));
		if (drawRight)
			drawList->AddImageQuad(textureID, { p2.x, p1.y - widthOffset }, { p2.x + radius, p1.y - widthOffset }, { p2.x + radius, p2.y + widthOffset }, { p2.x, p2.y + widthOffset },
				{ 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, ImColor(0.0f, 0.0f, 0.0f, alphaSides));

		drawList->PopClipRect();
	};

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