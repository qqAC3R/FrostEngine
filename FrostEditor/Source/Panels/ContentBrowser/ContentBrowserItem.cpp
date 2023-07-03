#include "frostpch.h"
#include "ContentBrowserList.h"

#include "Frost/Core/Input.h"
#include "Frost/Project/Project.h"
#include "Frost/InputCodes/KeyCodes.h"
#include "Frost/Utils/FileSystem.h"

#include "UserInterface/UIWidgets.h"
#include "Frost/Asset/AssetExtensions.h"
#include "Frost/Asset/AssetManager.h"
#include "Panels/ContentBrowser/ContentBrowser.h"

#include <imgui_internal.h>

#define MAX_INPUT_BUFFER_LENGTH 128

namespace Frost
{
	static char s_RenameBuffer[MAX_INPUT_BUFFER_LENGTH];

	ContentBrowserItem::ContentBrowserItem(UUID itemID, ItemType type, const std::filesystem::path& filepath, ContentBrowserPanel* contentBrowserPanel)
		: m_ID(itemID), m_Type(type), m_Name(filepath.filename().string()), m_Filepath(filepath), m_Extension(filepath.extension().string()), m_ContentBrowserPanel(contentBrowserPanel)
	{
		m_AssetType = AssetType::None;
		if (s_AssetExtensionMap.find(m_Extension) != s_AssetExtensionMap.end())
			m_AssetType = s_AssetExtensionMap[m_Extension];
	}

	void ContentBrowserItem::OnRenderBegin()
	{
		ImGui::PushID(m_Name.c_str());
		ImGui::BeginGroup();
	}

	static ImU32 GetInfoPanelAssetTypeColo(AssetType assetType)
	{
		switch (assetType)
		{
			case AssetType::None:        return IM_COL32(64, 127, 254, 255); // Blue (Default)
			case AssetType::Scene:       return IM_COL32(202, 114, 0, 255); // Orange
			case AssetType::MeshAsset:   return IM_COL32(92, 184, 255, 255); // Light Blue
			case AssetType::Material:    return IM_COL32(25, 255, 64, 255); // Light Green (lime)
			case AssetType::Prefab:      return IM_COL32(249, 51, 255, 255); // Pink
			case AssetType::PhysicsMat:  return IM_COL32(252, 58, 58, 255); // Red
			case AssetType::Texture:     return IM_COL32(207, 252, 5, 255); // Yellowish green
			case AssetType::EnvMap:      return IM_COL32(58, 252, 178, 255); // Cyan
			case AssetType::Font:        return IM_COL32(128, 0, 130, 255); // Dark pink
		}
		return IM_COL32(64, 127, 254, 255);
	}

	ContentBrowserItemAction ContentBrowserItem::OnRender(float thumbnailSize, bool displayAssetType, bool isItemCutAndPasted)
	{
		ContentBrowserItemAction result;
		result.Set(ContentBrowserAction::None, true);

		//UserInterface::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

		const float edgeOffset = 4.0f;

		const float textLineHeight = ImGui::GetTextLineHeightWithSpacing() * 2.0f + edgeOffset * 2.0f;
		const float infoPanelHeight = std::max(displayAssetType ? thumbnailSize * 0.5f : textLineHeight, textLineHeight);

		const ImVec2 topLeft = ImGui::GetCursorScreenPos();
		const ImVec2 thumbBottomRight = { topLeft.x + thumbnailSize, topLeft.y + thumbnailSize };
		const ImVec2 infoTopLeft = { topLeft.x,				 topLeft.y + thumbnailSize };
		const ImVec2 bottomRight = { topLeft.x + thumbnailSize, topLeft.y + thumbnailSize + infoPanelHeight };

		auto drawShadow = [](const ImVec2& topLeft, const ImVec2& bottomRight, bool directory)
		{
			auto* drawList = ImGui::GetWindowDrawList();

			ImRect rect(topLeft, bottomRight);
			rect.Min.x += 1.0f;
			rect.Min.y += 1.0f;
			rect.Max.x += 1.0f;
			rect.Max.y += 1.0f;

			const ImRect itemRect = rect;

			ImU32 propertyFieldColor = IM_COL32(15, 15, 15, 255);

			drawList->AddRect(itemRect.Min, itemRect.Max, propertyFieldColor, 6.0f, directory ? 0 : ImDrawFlags_RoundCornersBottom, 2.0f);
		};

		std::string invisibleButtonID = "##" + m_Name + "_InvisibleButton";
		UserInterface::InvisibleButtonWithNoBehaivour(invisibleButtonID.c_str(), { bottomRight.x - topLeft.x, infoTopLeft.y - topLeft.y + infoPanelHeight });
		ImGui::SetCursorScreenPos(topLeft);

		/// Draw buttons
		if (m_Type != ItemType::Directory)
		{
			auto* drawList = ImGui::GetWindowDrawList();

			// Draw shadow
			drawShadow(topLeft, bottomRight, false);

			constexpr auto backgroundDarkColor = IM_COL32(26, 26, 26, 255);
			constexpr auto groupHeaderColor = IM_COL32(47, 47, 47, 255);
			//constexpr auto blueColor = IM_COL32(22, 139, 247, 255);

			// Draw background
			drawList->AddRectFilled(topLeft, thumbBottomRight, backgroundDarkColor);
			drawList->AddRectFilled(infoTopLeft, bottomRight, groupHeaderColor, 6.0f, ImDrawFlags_RoundCornersBottom);
			drawList->AddRectFilled(infoTopLeft, { bottomRight.x, infoTopLeft.y + 2.0f }, GetInfoPanelAssetTypeColo(m_AssetType));
		}
		else if (ImGui::ItemHoverable(ImRect(topLeft, bottomRight), ImGui::GetID(m_Name.c_str())) || m_IsSelected)
		{
			// Draw shadow
			drawShadow(topLeft, bottomRight, true);

			constexpr auto groupHeaderColor = IM_COL32(47, 47, 47, 255);

			auto* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(topLeft, bottomRight, groupHeaderColor, 6.0f);
		}

		/// Thumbnail
		{
			ImGui::InvisibleButton("##thumbnailButton", ImVec2{ thumbnailSize, thumbnailSize });

			float x = -6.0f;
			float y = -6.0f;

			ImRect result = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
			result.Min.x -= x;
			result.Min.y -= y;
			result.Max.x += x;
			result.Max.y += y;

			Ref<Texture2D> icon = ContentBrowserPanel::GetIconByAssetType(m_AssetType);
			if(m_Type == ItemType::Directory)
				icon = ContentBrowserPanel::GetFolderIcon();

			ImU32 thumbnailColorTint = IM_COL32(255, 255, 255, 225);

			if(isItemCutAndPasted)
				thumbnailColorTint = IM_COL32(255, 255, 255, 150);

			UserInterface::DrawButtonImage(icon,
				thumbnailColorTint,
				thumbnailColorTint,
				thumbnailColorTint,
				result
			);
		}
		
		
		/// Info Panel

		auto renamingWidget = [&]
		{
			ImGui::SetKeyboardFocusHere();
			if (ImGui::InputText("##rename", s_RenameBuffer, MAX_INPUT_BUFFER_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				OnRenamed(s_RenameBuffer);
				m_IsRenaming = false;
				result.Set(ContentBrowserAction::Renamed, true);
				result.Set(ContentBrowserAction::Refresh, true);

			}
		};

		const ImVec2 cursor = ImGui::GetCursorPos();
		ImGui::SetCursorPos(ImVec2(cursor.x + edgeOffset , cursor.y + edgeOffset));

		if (m_Type == ItemType::Directory)
		{
			{
				const float textWidth = std::min(ImGui::CalcTextSize(m_Name.c_str()).x, thumbnailSize);
				const float textCenter = ((thumbnailSize / 2.0f) - (textWidth / 2.0f)) - 1.0f;
				ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +  (thumbnailSize - edgeOffset * 3.0f) );

				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textCenter);

				if (m_IsRenaming)
				{
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() - textCenter / 2.0f);
					ImGui::SetNextItemWidth(textWidth + textCenter);
					renamingWidget();
				}
				else
				{
					ImGui::SetNextItemWidth(textWidth);
					ImGui::Text(m_Name.c_str());
				}
				ImGui::PopTextWrapPos();
			}
		}
		else
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (thumbnailSize - edgeOffset * 2.0f));
			if (m_IsRenaming)
			{
				ImGui::SetNextItemWidth(thumbnailSize - edgeOffset * 3.0f);
				renamingWidget();
			}
			else
			{
				ImGui::Text(m_Name.c_str());
			}
			ImGui::PopTextWrapPos();
			

			{
				// Display asset type

				const std::string& assetType = Utils::AssetTypeToString(m_AssetType);
				const float textWidth = std::min(ImGui::CalcTextSize(assetType.c_str()).x, thumbnailSize);
				
				//ImGui::SetCursorPos(ImVec2(bottomRight.x - textWidth, bottomRight.y - 10.0f));
				ImGui::SetCursorPos(ImVec2(cursor.x + (thumbnailSize - textWidth - edgeOffset), cursor.y + (infoPanelHeight - 20.0f)));

				ImU32 darkerFontText = IM_COL32(120, 120, 120, 200);
				UserInterface::ScopedStyle textColour(ImGuiCol_Text, darkerFontText);
				ImGui::TextUnformatted(assetType.c_str());
			}
		}

		if (!m_IsRenaming)
		{
			if (Input::IsKeyPressed(Key::F2) && m_IsSelected)
				StartRenaming();
		}
		ImGui::PopStyleVar(); // ItemSpacing


		/// End of the Item Group
		//======================
		ImGui::EndGroup();

		

		/// Draw outline
		//======================
		if (m_IsSelected || ImGui::IsItemHovered())
		{
			ImRect itemRect = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
			auto* drawList = ImGui::GetWindowDrawList();

			itemRect.Max.y = bottomRight.y;

			if (m_IsSelected)
			{
				constexpr ImU32 selectionColor = IM_COL32(237, 192, 119, 255);

				const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsItemHovered();
				ImColor colTransition = IM_COL32(237 * 0.8, 192 * 0.8, 119 * 0.8, 255 * 0.8);

				drawList->AddRect(itemRect.Min, itemRect.Max,
					mouseDown ? colTransition : selectionColor, 6.0f,
					m_Type == ItemType::Directory ? 0 : ImDrawFlags_RoundCornersBottom, 1.0f);
			}
			else // isHovered
			{
				if (m_Type != ItemType::Directory)
				{
					constexpr ImU32 mutedColor = IM_COL32(77, 77, 77, 255);

					drawList->AddRect(itemRect.Min, itemRect.Max,
						mutedColor, 6.0f,
						ImDrawFlags_RoundCornersBottom, 1.0f);
				}
			}
		}


		/// Mouse events
		//==================

		bool dragging = false;
		if (dragging = ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			m_IsDragging = true;

			SelectionStack* selectionStack = &m_ContentBrowserPanel->m_SelectionStack;
			if (!selectionStack->IsSelected(m_ID))
				result.Set(ContentBrowserAction::ClearSelection, true);

			auto& currentItems = m_ContentBrowserPanel->m_CurrentItems;

			if (selectionStack->SelectionCount() > 0)
			{
				for (const auto& selectedItemHandles : *selectionStack)
				{
					size_t index = currentItems.FindItem(selectedItemHandles.Handle);
					if (index == ContentBrowserItemList::InvalidItem)
						continue;

					const auto& item = currentItems[index];
					ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
					ImTextureID textureID = imguiLayer->GetImGuiTextureID(ContentBrowserPanel::GetIconByAssetType(m_AssetType)->GetImage2D());
					ImGui::Image(textureID, ImVec2(20, 20));


					ImGui::SameLine();
					const auto& name = item->GetName();
					ImGui::TextUnformatted(name.c_str());

					//ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
				}

				ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, selectionStack->Data(), sizeof(SelectionData) * selectionStack->SelectionCount());
			}

			result.Set(ContentBrowserAction::Selected, true);
			ImGui::EndDragDropSource();
		}


		if (ImGui::IsItemHovered())
		{
			result.Set(ContentBrowserAction::Hovered, true);


			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				result.Set(ContentBrowserAction::NavigateToThis, true);
			}
			else
			{
				bool action = m_ContentBrowserPanel->m_SelectionStack.SelectionCount() > 1 ? ImGui::IsMouseReleased(ImGuiMouseButton_Left) : ImGui::IsMouseClicked(ImGuiMouseButton_Left);
				bool skipBecauseDragging = m_IsDragging;

				if (action && !skipBecauseDragging)
				{
					result.Set(ContentBrowserAction::Selected, true);

					if (!Input::IsKeyPressed(Key::LeftControl) && !Input::IsKeyPressed(Key::LeftShift))
						result.Set(ContentBrowserAction::ClearSelection, true);
					
					if (Input::IsKeyPressed(Key::LeftShift))
						result.Set(ContentBrowserAction::LeftShiftSelection, true);
				}
			}
		}

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
		if (ImGui::BeginPopupContextItem("CBItemContextMenu"))
		{
			result.Set(ContentBrowserAction::Selected, true);

			//if (!Input::IsKeyPressed(Key::LeftControl) && !Input::IsKeyPressed(Key::LeftShift))
			//	result.Set(ContentBrowserAction::ClearSelection, true);

			OnContextMenuOpen(result);
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();


		





		m_IsDragging = dragging;

		return result;
	}

	void ContentBrowserItem::OnRenderEnd()
	{
		ImGui::PopID();
		ImGui::NextColumn();
	}

	void ContentBrowserItem::SetSelected(bool value)
	{
		m_IsSelected = value;
	}

	void ContentBrowserItem::StartRenaming()
	{
		if (m_IsRenaming)
			return;

		memset(s_RenameBuffer, 0, MAX_INPUT_BUFFER_LENGTH);
		memcpy(s_RenameBuffer, m_Name.c_str(), m_Name.size());
		m_IsRenaming = true;
	}

	void ContentBrowserItem::StopRenaming()
	{
		m_IsRenaming = false;
		memset(s_RenameBuffer, 0, MAX_INPUT_BUFFER_LENGTH);
	}

	void ContentBrowserItem::OnContextMenuOpen(ContentBrowserItemAction& actionResult)
	{

		if (ImGui::MenuItem("Reload Asset"))
			actionResult.Set(ContentBrowserAction::ReloadAsset, true);

		if (ImGui::MenuItem("Rename", "F2"))
		{
			StartRenaming();
			actionResult.Set(ContentBrowserAction::Renamed, true);
		}

		if (ImGui::MenuItem("Copy", "Ctrl+C"))
			actionResult.Set(ContentBrowserAction::Copy, true);

		if (ImGui::MenuItem("Delete"))
			actionResult.Set(ContentBrowserAction::Delete, true);


		ImGui::Separator();

		if (ImGui::MenuItem("Show In Explorer"))
			actionResult.Set(ContentBrowserAction::ShowInExplorer, true);
		if (ImGui::MenuItem("Open Externally"))
			actionResult.Set(ContentBrowserAction::OpenExternal, true);
	}

	void ContentBrowserItem::OnRenamed(const std::string& newName)
	{
		if (FileSystem::Exists(Project::GetActive()->GetAssetDirectory() / m_Filepath.parent_path() / newName))
		{
			FROST_CORE_ERROR("A directory with that name already exists!");
			return;
		}

		FileSystem::Rename(Project::GetActive()->GetAssetDirectory() / m_Filepath, Project::GetActive()->GetAssetDirectory() / m_Filepath.parent_path() / newName);

		if (m_Type == ItemType::Directory)
		{

			AssetManager::OnRenameFilepath(
				Project::GetActive()->GetAssetDirectory() / m_Filepath,
				Project::GetActive()->GetAssetDirectory() / m_Filepath.parent_path() / newName
			);
		}
		else
		{
			AssetManager::OnRenameAsset(
				Project::GetActive()->GetAssetDirectory() / m_Filepath,
				newName
			);
		}

		

	}

	void ContentBrowserItem::OnDelete()
	{
		bool deleted = FileSystem::DeleteFile(Project::GetActive()->GetAssetDirectory() / m_Filepath);
		if (!deleted)
		{
			FROST_CORE_ERROR("Failed to delete folder {0}", m_Filepath.string());
			return;
		}

		if (m_Type == ItemType::Directory)
		{
			for (auto& [assetUUID, assetInfo] : m_ContentBrowserPanel->m_CurrentDirectory->Files)
			{
				const AssetMetadata& assetMetadata = AssetManager::GetMetadata(assetInfo->FilePath);
				AssetManager::OnAssetDeleted(assetMetadata.Handle);
			}
		}
		else
		{
			const AssetMetadata& assetMetadata = AssetManager::GetMetadata(m_Filepath);
			AssetManager::OnAssetDeleted(assetMetadata.Handle);
		}
	}

}