#include "frostpch.h"
#include "ContentBrowser.h"

#include "Frost/Core/Input.h"
#include "Frost/Core/Application.h"
#include "Frost/Project/Project.h"
#include "Frost/InputCodes/KeyCodes.h"

#include "Frost/EntitySystem/Prefab.h"

#include "UserInterface/UIWidgets.h"
#include "Frost/ImGui/Utils/CustomTreeNode.h"
#include "Frost/ImGui/Utils/ScopedStyle.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Frost
{
	std::map<std::string, Frost::Ref<Frost::Texture2D>> ContentBrowserPanel::m_AssetIconMap;

	ContentBrowserPanel::ContentBrowserPanel()
	{
	}

	ContentBrowserPanel::~ContentBrowserPanel()
	{
	}

	void ContentBrowserPanel::InitAllAssetIcons()
	{
		TextureSpecification textureSpec{};
		textureSpec.FlipTexture = false;

		m_AssetIconMap["IconBack"] = Texture2D::Create("Resources/Editor/ContentBrowser/IconBack.png", textureSpec);
		m_AssetIconMap["IconForward"] = Texture2D::Create("Resources/Editor/ContentBrowser/IconFoward.png", textureSpec);
		m_AssetIconMap["IconRefresh"] = Texture2D::Create("Resources/Editor/ContentBrowser/Refresh.png", textureSpec);
		m_AssetIconMap["IconInputSearch"] = Texture2D::Create("Resources/Editor/ContentBrowser/InputSeach24px.png", textureSpec);
		m_AssetIconMap["IconFolder"] = Texture2D::Create("Resources/Editor/ContentBrowser/FolderClosed.png", textureSpec);

		m_AssetIconMap["IconMeshAsset"] = Texture2D::Create("Resources/Editor/ContentBrowser/FBXAssetIcon.png", textureSpec);
		m_AssetIconMap["IconFont"] = Texture2D::Create("Resources/Editor/ContentBrowser/FontIcon.png", textureSpec);
		m_AssetIconMap["IconMaterialAsset"] = Texture2D::Create("Resources/Editor/ContentBrowser/MaterialAssetIcon.png", textureSpec);
		m_AssetIconMap["IconPrefab"] = Texture2D::Create("Resources/Editor/ContentBrowser/PrefabIcon.png", textureSpec);

		m_AssetIconMap["IconTexturePNG"] = Texture2D::Create("Resources/Editor/ContentBrowser/TexturePNG.png", textureSpec);
		m_AssetIconMap["IconTextureJPG"] = Texture2D::Create("Resources/Editor/ContentBrowser/TextureJPG.png", textureSpec);

		m_AssetIconMap["IconScene"] = Texture2D::Create("Resources/Editor/ContentBrowser/Scene.png", textureSpec);

		m_AssetIconMap["IconDefaultFile"] = Texture2D::Create("Resources/Editor/ContentBrowser/DefaultFileIcon.png", textureSpec);

		m_AssetIconMap["SideContentShadow"] = Texture2D::Create("Resources/Editor/ContentBrowser/SideContentShadow.png", textureSpec);
	}

	void ContentBrowserPanel::DeleteAllAssetIcons()
	{
		m_AssetIconMap.clear();
	}

	void ContentBrowserPanel::Init(void* initArgs)
	{
		//m_Directories[0] = nullptr;
		UUID baseDirectoryUUID = ProcessDirectory(Project::GetAssetDirectory(), nullptr);
		m_BaseDirectory = m_Directories[baseDirectoryUUID];
		m_CurrentDirectory = m_BaseDirectory;
		ChangeCurrentDirectory(m_CurrentDirectory);

		if (m_AssetIconMap.empty())
			InitAllAssetIcons();

		memset(m_SearchBuffer, 0, 128 * sizeof(char));
	}

	void ContentBrowserPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& event) { return OnKeyPressedEvent(event); });
	}

	Ref<Texture2D> ContentBrowserPanel::GetIconByAssetType(AssetType assetType)
	{
		switch (assetType)
		{
			case AssetType::MeshAsset: return m_AssetIconMap["IconMeshAsset"];
			case AssetType::Material:  return m_AssetIconMap["IconMaterialAsset"];
			case AssetType::Prefab:    return m_AssetIconMap["IconPrefab"];
			case AssetType::Font:      return m_AssetIconMap["IconFont"];
			case AssetType::Texture:   return m_AssetIconMap["IconTextureJPG"];
			case AssetType::Scene:     return m_AssetIconMap["IconScene"];
			default:                   return m_AssetIconMap["IconDefaultFile"];
		}
		return nullptr;
	}

	Ref<Texture2D> ContentBrowserPanel::GetFolderIcon()
	{
		return m_AssetIconMap["IconFolder"];
	}

	static float s_Padding = 2.0f;
	static float s_ThumbnailSize = 128.0f;

	void ContentBrowserPanel::Render()
	{
		if (!m_Visibility) return;

		ImGui::Begin("Asset Browser", NULL, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);

		{

			ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable
				| ImGuiTableFlags_SizingFixedFit
				| ImGuiTableFlags_BordersInnerV;

			if (ImGui::BeginTable("ContentBrowserTable", 2, tableFlags, ImVec2(0.0f, 0.0f)))
			{
				
				ImGui::TableSetupColumn("Outliner", 0, 300.0f);
				ImGui::TableSetupColumn("Directory Structure", ImGuiTableColumnFlags_WidthStretch);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);


				ImGui::BeginChild("DirTree");
				{
					if (ImGui::CollapsingHeader("Content", nullptr, ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::ScopedStyle spacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
						ImGui::ScopedStyle itemBg(ImGuiCol_Header, IM_COL32_DISABLE);
						ImGui::ScopedStyle itemBg2(ImGuiCol_HeaderActive, IM_COL32_DISABLE);

						RenderDirectoryHierarchy(m_BaseDirectory, 0);
					}

					// Render side shadow
					{
						float x = 0.0f;
						float y = 10.0f;
						ImRect windowRect = ImGui::GetCurrentWindow()->Rect();
						windowRect.Min.x -= x;
						windowRect.Min.y -= y;
						windowRect.Max.x += x;
						windowRect.Max.y += y;

						ImGui::PushClipRect(windowRect.Min, windowRect.Max, false);
						UserInterface::DrawShadowInner(m_AssetIconMap["SideContentShadow"], 20.0f, windowRect, 1.0f, windowRect.GetHeight() / 4.0f, false, true, false, false);
						ImGui::PopClipRect();
					}
									}
				ImGui::EndChild(); // DirTree


				ImGui::TableSetColumnIndex(1);


				{

					ImGui::ScopedStyle colorChildBg(ImGuiCol_ChildBg, ImVec4(0.09f, 0.12f, 0.15f, 1.0f));

					ImGui::BeginChild("Content");
					{
						{
							ImGui::ScopedStyle frameBorderSize(ImGuiStyleVar_FrameBorderSize, 0.0f);

							const float topBarHeight = 24.0f;
							RenderTopArea(topBarHeight);
						}

						ImGui::Separator();

						

						// This Right/Left padding was mostly set to see the DragDrop border
						float contentPadding = 5.0f;
						float availableWidth = ImGui::GetContentRegionAvail().x;
						float childWidth = availableWidth - (2 * contentPadding);

						// Set the cursor position to add padding from the left
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + contentPadding);

						ImGui::BeginChild("Scrolling", { childWidth, 0});
						{

							ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
							if (ImGui::BeginPopupContextWindow(0, 1, false))
							{
								if (ImGui::BeginMenu("New"))
								{
									if (ImGui::MenuItem("Folder"))
									{
										bool created = FileSystem::CreateDirectory(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / "New Folder");
										if (created)
										{
											RefreshContent();
											const auto& directoryInfo = GetDirectory(m_CurrentDirectory->FilePath / "New Folder");
											size_t index = m_CurrentItems.FindItem(directoryInfo->Handle);
											if (index != ContentBrowserItemList::InvalidItem)
												m_CurrentItems[index]->StartRenaming();
										}
									}

									if (ImGui::MenuItem("Scene"))
										CreateAsset<Scene>("New Scene.fsc");

									if (ImGui::MenuItem("Material"))
										CreateAsset<MaterialAsset>("New Material.fmat");

									if (ImGui::MenuItem("Physics Material"))
										CreateAsset<PhysicsMaterial>("New Physics Material.fpmat");

									ImGui::EndMenu();
								}

#if 0
								if (ImGui::MenuItem("Import"))
								{
									std::string filepath = Application::Get().OpenFile();
									if (!filepath.empty())
										FileSystem::MoveFile(filepath, m_CurrentDirectory->FilePath);
								}
#endif

								if (ImGui::MenuItem("Refresh"))
									RefreshContent();

								if (ImGui::MenuItem("Copy", "Ctrl+C", nullptr, m_SelectionStack.SelectionCount() > 0))
									m_CopiedAssets.CopyFrom(m_SelectionStack);

								if (ImGui::MenuItem("Paste", "Ctrl+V", nullptr, m_CopiedAssets.SelectionCount() > 0))
									PasteCopiedAssets();

								if (ImGui::MenuItem("Duplicate", "Ctrl+D", nullptr, m_SelectionStack.SelectionCount() > 0))
								{
									m_CopiedAssets.CopyFrom(m_SelectionStack);
									PasteCopiedAssets();
								}

								ImGui::Separator();

								if (ImGui::MenuItem("Show in Explorer"))
									FileSystem::OpenDirectoryInExplorer(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath);

								ImGui::EndPopup();
							}
							ImGui::PopStyleVar(); // ItemSpacing







							const float paddingForOutline = 2.0f;
							const float scrollBarrOffset = 20.0f + ImGui::GetStyle().ScrollbarSize;
							float panelWidth = ImGui::GetContentRegionAvail().x - scrollBarrOffset;
							float cellSize = s_ThumbnailSize + s_Padding + paddingForOutline;
							int columnCount = (int)(panelWidth / cellSize);
							if (columnCount < 1) columnCount = 1;

							{
								// TODO: Maybe: rowSpacing = 12.0f
								const float rowSpacing = 12.0f;
								ImGui::ScopedStyle itemSpacing(ImGuiStyleVar_ItemSpacing, ImVec2(paddingForOutline, rowSpacing));
								ImGui::Columns(columnCount, 0, false);

								ImGui::ScopedStyle border(ImGuiStyleVar_FrameBorderSize, 0.0f);
								ImGui::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

								RenderItems();
							}

							UpdateInput();

							RenderDeleteDialogue();
						}
						ImGui::EndChild(); // Scrolling
						UpdateDragDropTargetWithAssets();

					}
					ImGui::EndChild(); // Content


					


					ImGui::EndTable();
				}
			}

		}


		// Check if the window is focused (also including child window)
		m_IsContentBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
		m_IsContentBrowserHovered = ImGui::IsWindowHovered(ImGuiFocusedFlags_ChildWindows);

		ImGui::End();
	}

	void ContentBrowserPanel::UpdateDragDropTargetWithAssets()
	{
		if (ImGui::BeginDragDropTarget())
		{
			auto data = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_DRAG_DROP);
			if (data)
			{
				//Entity& e = *(Entity*)data->Data;
				ContentBrowserDragDropData& dragDropData = *(ContentBrowserDragDropData*)data->Data;


				// TODO: Maybe add also Scene saving directly from the hierarchy panel to the content browser
				switch (dragDropData.AssetType)
				{
				case AssetType::Prefab:
				{
					Entity& e = *(Entity*)dragDropData.Data;
					Ref<Prefab> prefab = CreateAsset<Prefab>("New Prefab.fprefab");
					prefab->Create(e);
					AssetImporter::Serialize(prefab);

					break;
				}
				case AssetType::Material:
				{
					MaterialAsset* materialAssetSource = (MaterialAsset*)dragDropData.Data;

					Ref<MaterialAsset> materialAsset = CreateAsset<MaterialAsset>("New Material.fmat");
					materialAsset->CopyFrom(materialAssetSource);
					AssetImporter::Serialize(materialAsset);

					break;
				}
				case AssetType::PhysicsMat:
				{
					PhysicsMaterial* phyiscsMatAssetSource = (PhysicsMaterial*)dragDropData.Data;

					Ref<PhysicsMaterial> phyiscsMatAsset = CreateAsset<PhysicsMaterial>("New Physics Material.fpmat");
					phyiscsMatAsset->CopyFrom(phyiscsMatAssetSource);
					AssetImporter::Serialize(phyiscsMatAsset);

					break;
				}
				case AssetType::AnimationBlueprint:
				{
					AnimationBlueprint* animBlueprintAssetSource = (AnimationBlueprint*)dragDropData.Data;

					Ref<AnimationBlueprint> animBlueprintAsset = CreateAsset<AnimationBlueprint>("New Animation Blueprint.fanim", (void*)animBlueprintAssetSource->GetAppropiateMeshAsset());
					animBlueprintAsset->Copy(animBlueprintAssetSource);
					AssetImporter::Serialize(animBlueprintAsset);

					break;
				}
				default:
					break;
				}

			}
			ImGui::EndDragDropTarget();
		}
	}

	void ContentBrowserPanel::UpdateInput()
	{
		if (!m_IsContentBrowserHovered)
			return;

		if (((!m_IsAnyItemHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left)) || Input::IsKeyPressed(Key::Escape)) && !m_IsDeleteDialogueOpened)
			ClearSelections();

		if (Input::IsKeyPressed(Key::Delete) && m_SelectionStack.SelectionCount() > 0)
			ImGui::OpenPopup("Delete");

		if (Input::IsKeyPressed(Key::F5))
			RefreshContent();
	}

	void ContentBrowserPanel::Shutdown()
	{
		DeleteAllAssetIcons();
	}

	namespace UI
	{
		static bool TreeNode(const std::string& id, const std::string& label, ImGuiTreeNodeFlags flags = 0, const Ref<Texture2D>& icon = nullptr)
		{
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return false;

			return ImGui::TreeNodeWithIcon(icon, window->GetID(id.c_str()), flags, label.c_str(), NULL);
		}
	}

	void ContentBrowserPanel::RenderDirectoryHierarchy(const Ref<DirectoryInfo>& directoryInfo, uint32_t level)
	{
		ImGuiTreeNodeFlags leafFlag = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth;

		ImGui::ScopedStyle framePadding(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 10.0f));
		ImGui::ScopedStyle headerColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));

		// Draw directories
		for (auto& [filepath, directoryInfo] : directoryInfo->ArrangedSubDirectories)
		{
			std::string name = directoryInfo->FilePath.filename().string();
			std::string id = name + "_TreeNode";

			{
				bool opened = false;

				if (directoryInfo->Handle == m_CurrentDirectory->Handle)
				{
					ImGui::ScopedStyle headerColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
					opened = UI::TreeNode(id,
						name,
						ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_Selected,
						m_AssetIconMap["IconFolder"]
					);
				}
				else
				{
					opened = UI::TreeNode(id,
						name,
						ImGuiTreeNodeFlags_OpenOnArrow,
						m_AssetIconMap["IconFolder"]
					);
				}

				if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered())
				{
					m_CurrentDirectory = directoryInfo;
					//RefreshContent();
					ChangeCurrentDirectory(m_CurrentDirectory);
				}

				if (opened)
				{
					RenderDirectoryHierarchy(directoryInfo, level + 1);
					ImGui::TreePop();
				}
			}
		}

		// Draw files
		for (auto& [fileUUID, fileInfo] : directoryInfo->ArrangedFiles)
		{
			std::string name = fileInfo->FilePath.filename().string();
			std::string id = name + "_TreeNode";

			{
				UI::TreeNode(id,
					name,
					leafFlag | ImGuiTreeNodeFlags_Leaf,
					GetIconByAssetType(fileInfo->AssetType)
				);

				// Setting up the drag and drop source for the filepath.
				if (ImGui::BeginDragDropSource())
				{
					ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();
					ImTextureID textureID = imguiLayer->GetImGuiTextureID(ContentBrowserPanel::GetIconByAssetType(fileInfo->AssetType)->GetImage2D());
					ImGui::Image(textureID, ImVec2(20, 20));

					ImGui::SameLine();
					ImGui::TextUnformatted(name.c_str());

					// Set the payload.
					ImGui::SetDragDropPayload(CONTENT_BROWSER_DRAG_DROP, fileInfo.Raw(), sizeof(FileInfo));
					ImGui::EndDragDropSource();
				}
			}
		}
	}

	void ContentBrowserPanel::RenderItems()
	{
		bool refreshAfterItemRendering = false;
		m_IsAnyItemHovered = false;

		for (auto& item : m_CurrentItems)
		{
			if (std::strlen(m_SearchBuffer) > 0)
			{
				std::string searchBufferToLower = m_SearchBuffer;
				std::transform(searchBufferToLower.begin(), searchBufferToLower.end(), searchBufferToLower.begin(), ::tolower);

				std::string itemNameToLower = item->GetName();
				std::transform(itemNameToLower.begin(), itemNameToLower.end(), itemNameToLower.begin(), ::tolower);

				if (itemNameToLower.find(searchBufferToLower) == std::string::npos)
					continue;
			}


			item->OnRenderBegin();

			ContentBrowserItemAction result = item->OnRender(
				s_ThumbnailSize, // Thumbnail Size
				true, // Display Asset Type
				m_CutAndPasteAssets.IsSelected(item->GetID()) // Is Item CutAndPasted
			);

			if (result.IsSet(ContentBrowserAction::ClearSelection))
				ClearSelections();

			if (result.IsSet(ContentBrowserAction::Copy))
			{
				m_CopiedAssets.Select(item->GetID(), item->m_Type, item->m_AssetType, item->m_Filepath);
				if (m_SelectionStack.SelectionCount() > 1)
				{
					m_CopiedAssets.CopyFrom(m_SelectionStack);
				}
			}

			if (result.IsSet(ContentBrowserAction::Delete))
				m_OpenDeleteDialogue = true;

			if (result.IsSet(ContentBrowserAction::Hovered))
				m_IsAnyItemHovered = true;

			if (result.IsSet(ContentBrowserAction::ReloadAsset))
				AssetManager::ReloadData(AssetManager::GetMetadata(item->GetFilepath()).Handle);

			if (result.IsSet(ContentBrowserAction::Selected) && !m_SelectionStack.IsSelected(item->GetID()))
			{
				m_SelectionStack.Select(item->GetID(), item->m_Type, item->m_AssetType, item->GetFilepath());
				item->SetSelected(true);
			}

			if (result.IsSet(ContentBrowserAction::LeftShiftSelection) && m_SelectionStack.SelectionCount() == 2)
			{
				size_t firstIndex = m_CurrentItems.FindItem(m_SelectionStack[0].Handle);
				size_t lastIndex = m_CurrentItems.FindItem(item->GetID());

				if (firstIndex > lastIndex)
				{
					size_t temp = firstIndex;
					firstIndex = lastIndex;
					lastIndex = temp;
				}

				for (size_t i = firstIndex; i <= lastIndex; i++)
				{
					auto toSelect = m_CurrentItems[i];
					toSelect->SetSelected(true);
					m_SelectionStack.Select(toSelect->GetID(), item->m_Type, toSelect->m_AssetType, toSelect->GetFilepath());
				}
			}

			if (result.IsSet(ContentBrowserAction::ShowInExplorer))
			{
				if (item->GetType() == ContentBrowserItem::ItemType::Directory)
					FileSystem::ShowFileInExplorer(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / item->GetName());
				else
					FileSystem::ShowFileInExplorer(Project::GetAssetDirectory() / item->GetFilepath());
			}

			if (result.IsSet(ContentBrowserAction::OpenExternal))
			{
				if (item->GetType() == ContentBrowserItem::ItemType::Directory)
					FileSystem::OpenExternally(Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / item->GetName());
				else
					FileSystem::OpenExternally(Project::GetAssetDirectory() / item->GetFilepath());
			}

			item->OnRenderEnd();

			

			if (result.IsSet(ContentBrowserAction::NavigateToThis))
			{
				// TODO: Clicking only works for directories now, but we could also open other engine files to modify them
				if (item->m_Type == ContentBrowserItem::ItemType::Directory)
				{
					m_CurrentDirectory = m_Directories[item->GetID()];
					ChangeCurrentDirectory(m_Directories[item->GetID()]);
					break;
				}
			}

			if (result.IsSet(ContentBrowserAction::Refresh))
				refreshAfterItemRendering = true;


		}

		if (refreshAfterItemRendering)
			RefreshContent();

		// This is a workaround an issue with ImGui: https://github.com/ocornut/imgui/issues/331
		if (m_OpenDeleteDialogue)
		{
			ImGui::OpenPopup("Delete");
			m_OpenDeleteDialogue = false;
		}
	}

	void ContentBrowserPanel::RefreshAllDirectories()
	{
		m_CurrentItems.Clear();
		m_Directories.clear();

		Ref<DirectoryInfo> currentDirectory = m_CurrentDirectory;
		UUID baseDirectoryHandle = ProcessDirectory(Project::GetAssetDirectory().string(), nullptr);
		m_BaseDirectory = m_Directories[baseDirectoryHandle];
		m_CurrentDirectory = GetDirectory(currentDirectory->FilePath);

		if (!m_CurrentDirectory)
			m_CurrentDirectory = m_BaseDirectory; // Our current directory was removed

		ChangeCurrentDirectory(m_CurrentDirectory);
	}

	void ContentBrowserPanel::RefreshContent()
	{
		RefreshAllDirectories();
		ChangeCurrentDirectory(m_CurrentDirectory);

		m_SelectionStack.Clear();
	}

	UUID ContentBrowserPanel::ProcessDirectory(const std::filesystem::path& directoryPath, const Ref<DirectoryInfo>& parent)
	{
		Ref<DirectoryInfo> directoryInfo = Ref<DirectoryInfo>::Create();
		directoryInfo->Handle = UUID();
		if(parent)
			directoryInfo->Parent = parent;
		else
			directoryInfo->Parent = nullptr;

		if (directoryPath == Project::GetAssetDirectory())
		{
			directoryInfo->IsRoot = true;
			directoryInfo->FilePath = "";
		}
		else
		{
			directoryInfo->FilePath = std::filesystem::relative(directoryPath, Project::GetAssetDirectory());
		}

		for (auto entry : std::filesystem::directory_iterator(directoryPath))
		{
			// If it is a directory, then do a recursion, to check that directory's content
			if (entry.is_directory())
			{
				UUID subdirHandle = ProcessDirectory(entry.path(), directoryInfo);
				directoryInfo->SubDirectories[subdirHandle] = m_Directories[subdirHandle];

				directoryInfo->ArrangedSubDirectories[m_Directories[subdirHandle]->FilePath] = m_Directories[subdirHandle];
			}
			else
			{
				const AssetMetadata& assetMetadata = AssetManager::GetMetadata(entry);

				UUID fileUUID = UUID();
				Ref<FileInfo> assetFileInfo = Ref<FileInfo>::Create(fileUUID, assetMetadata.Type, entry);
				directoryInfo->Files[fileUUID] = assetFileInfo;
				directoryInfo->ArrangedFiles[entry] = assetFileInfo;
			}

		}

		m_Directories[directoryInfo->Handle] = directoryInfo;

		return directoryInfo->Handle;
	}

	void ContentBrowserPanel::ClearSelections()
	{
		for (auto& item : m_CurrentItems)
		{
 			item->SetSelected(false);

			if (item->IsRenaming())
				item->StopRenaming();
		}

		m_SelectionStack.Clear();
	}

	void ContentBrowserPanel::PasteCopiedAssets()
	{
		if (m_CopiedAssets.SelectionCount() == 0)
			return;

		for (SelectionData copiedAsset : m_CopiedAssets)
		{
			auto originalFilePath = Project::GetAssetDirectory();

			if (copiedAsset.ItemType == ContentBrowserItem::ItemType::Asset)
			{
				originalFilePath /= copiedAsset.FilePath;
				
				auto newFilepath = Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / (copiedAsset.FilePath.stem().string() + copiedAsset.FilePath.extension().string());

				if (std::filesystem::exists(newFilepath))
				{
					newFilepath.replace_filename(
						copiedAsset.FilePath.stem().string() + " (1)" + copiedAsset.FilePath.extension().string()
					);
					FROST_CORE_WARN("Asset already exists!");
				}

				std::filesystem::copy_file(originalFilePath, newFilepath);
			}
			if (copiedAsset.ItemType == ContentBrowserItem::ItemType::Directory)
			{
				originalFilePath /= copiedAsset.FilePath;

				auto newFilepath = Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / copiedAsset.FilePath.stem();

				if (std::filesystem::exists(newFilepath))
				{
					newFilepath.replace_filename(
						copiedAsset.FilePath.stem().string() + " (1)"
					);
					FROST_CORE_WARN("Asset already exists!");
				}

				std::filesystem::copy(originalFilePath, newFilepath);
			}
		}

		RefreshContent();
		m_SelectionStack.Clear();
		m_CopiedAssets.Clear();
	}

	void ContentBrowserPanel::PasteCutandPasteAssets()
	{
		if (m_CutAndPasteAssets.SelectionCount() == 0)
			return;

		for (SelectionData copiedAsset : m_CutAndPasteAssets)
		{
			auto originalFilePath = Project::GetAssetDirectory();

			if (copiedAsset.ItemType == ContentBrowserItem::ItemType::Asset)
			{
				originalFilePath /= copiedAsset.FilePath;

				auto newFilepath = Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / (copiedAsset.FilePath.stem().string() + copiedAsset.FilePath.extension().string());

				if (std::filesystem::exists(newFilepath))
				{
					newFilepath.replace_filename(
						copiedAsset.FilePath.stem().string() + " (1)" + copiedAsset.FilePath.extension().string()
					);
					FROST_CORE_WARN("Asset already exists!");
				}

				FileSystem::Move(originalFilePath, newFilepath);
				AssetManager::OnMoveAsset(originalFilePath, newFilepath);
			}
			if (copiedAsset.ItemType == ContentBrowserItem::ItemType::Directory)
			{
				originalFilePath /= copiedAsset.FilePath;

				auto newFilepath = Project::GetAssetDirectory() / m_CurrentDirectory->FilePath / copiedAsset.FilePath.stem();

				if (std::filesystem::exists(newFilepath))
				{
					newFilepath.replace_filename(
						copiedAsset.FilePath.stem().string() + " (1)"
					);
					FROST_CORE_WARN("Asset already exists!");
				}

				FileSystem::Move(originalFilePath, newFilepath);
				AssetManager::OnMoveFilepath(originalFilePath, newFilepath);
			}
		}

		RefreshContent();
		m_SelectionStack.Clear();
		m_CutAndPasteAssets.Clear();
	}

	void ContentBrowserPanel::RenderDeleteDialogue()
	{
		ImGui::SetNextWindowPos({ Application::Get().GetWindow().GetWidth() / 2.0f, Application::Get().GetWindow().GetHeight() / 2.0f });
		if (ImGui::BeginPopupModal("Delete", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_SelectionStack.SelectionCount() == 0)
				ImGui::CloseCurrentPopup();

			m_IsDeleteDialogueOpened = true;

			ImGui::Text("Are you sure you want to delete %d items?", m_SelectionStack.SelectionCount());

			float columnWidth = ImGui::GetContentRegionAvailWidth() / 4;


			ImGui::Columns(4, 0, false);
			ImGui::SetColumnWidth(0, columnWidth);
			ImGui::SetColumnWidth(1, columnWidth);
			ImGui::SetColumnWidth(2, columnWidth);
			ImGui::SetColumnWidth(3, columnWidth);
			ImGui::NextColumn();
			if (ImGui::Button("Yes", ImVec2(columnWidth, 0)))
			{
				for (SelectionData handle : m_SelectionStack)
				{
					size_t index = m_CurrentItems.FindItem(handle.Handle);
					if (index == ContentBrowserItemList::InvalidItem)
						continue;

					m_CurrentItems[index]->OnDelete();
					m_CurrentItems.Erase(handle.Handle);
				}

				m_SelectionStack.Clear();

				RefreshContent();
				ChangeCurrentDirectory(m_CurrentDirectory);

				ImGui::CloseCurrentPopup();
			}

			ImGui::NextColumn();
			ImGui::SetItemDefaultFocus();
			if (ImGui::Button("No", ImVec2(columnWidth, 0)))
				ImGui::CloseCurrentPopup();

			ImGui::NextColumn();
			ImGui::EndPopup();
		}
		else
		{
			m_IsDeleteDialogueOpened = false;
		}
	}

	bool ContentBrowserPanel::OnKeyPressedEvent(KeyPressedEvent& e)
	{
		bool handled = false;

		if (Input::IsKeyPressed(Key::LeftControl))
		{
			switch (e.GetKeyCode())
			{
			case Key::C:
				m_CutAndPasteAssets.Clear();
				m_CopiedAssets.CopyFrom(m_SelectionStack);
				handled = true;
				break;
			case Key::V:
				if (m_CopiedAssets.SelectionCount() > 0)
					PasteCopiedAssets();
				else if (m_CutAndPasteAssets.SelectionCount() > 0)
					PasteCutandPasteAssets();

				handled = true;
				break;
			case Key::D:
				m_CopiedAssets.CopyFrom(m_SelectionStack);
				PasteCopiedAssets();
				handled = true;
				break;
			case Key::X:
				m_CopiedAssets.Clear();
				m_CutAndPasteAssets.CopyFrom(m_SelectionStack);
				handled = true;
				break;
			}
		}

		return handled;
	}

	Ref<DirectoryInfo> ContentBrowserPanel::GetDirectory(const std::filesystem::path& filepath) const
	{
		if (filepath.string() == "" || filepath.string() == ".")
			return m_BaseDirectory;

		for (const auto& [handle, directory] : m_Directories)
		{
			if (directory->FilePath == filepath)
				return directory;
		}

		return nullptr;
	}

	void ContentBrowserPanel::ChangeCurrentDirectory(Ref<DirectoryInfo>& directory)
	{
		if (!directory)
			return;

		m_CurrentItems.Items.clear();

		for (auto& [subdirFilepath, subdir] : directory->ArrangedSubDirectories)
		{
			m_CurrentItems.Items.push_back(
				Ref<ContentBrowserItem>::Create(
					subdir->Handle,
					ContentBrowserItem::ItemType::Directory,
					subdir->FilePath,
					this
				)
			);
		}

		for (auto [fileFilepath, fileInfo] :directory->ArrangedFiles)
		{
			if (std::filesystem::exists(fileInfo->FilePath))
			{
				m_CurrentItems.Items.push_back(
					Ref<ContentBrowserItem>::Create(
						fileInfo->Handle,
						ContentBrowserItem::ItemType::Asset,
						fileInfo->FilePath,
						this
					)
				);
			}
		}

		m_SelectionStack.Clear();
	}

	void ContentBrowserPanel::GetAllDirectoriesForBreadcrumbs(Vector<Ref<DirectoryInfo>>& directories, const Ref<DirectoryInfo>& directory)
	{
		directories.push_back(directory);

		if (directory->Parent)
			GetAllDirectoriesForBreadcrumbs(directories, directory->Parent);
	}

	void ContentBrowserPanel::RenderTopArea(float height)
	{
		ImGui::ScopedStyle frameRounding(ImGuiStyleVar_FrameBorderSize, 3.0f);
		ImGui::ScopedStyle borderColor(ImGuiCol_Border, IM_COL32(50, 50, 50, 255));

		ImGui::BeginChild("ContentTopBar", ImVec2(0, height + 8.0f), true);
		{
			ImGui::ScopedStyle borderSize(ImGuiStyleVar_ChildBorderSize, 3.0f);
			ImGui::ScopedStyle frameBorderSize(ImGuiStyleVar_FrameBorderSize, 0.0f);

			auto contenBrowserButton = [height](const char* labelId, Ref<Texture2D> icon, bool disabled = false)
			{
				const float iconSize = std::min(24.0f, height);
				const float iconPadding = 3.0f;

				float x = -iconPadding;
				float y = -iconPadding;

				ImU32 activeButtonColor = IM_COL32(255, 255, 255, 255);
				ImU32 hoveredButtonColor = IM_COL32(200, 200, 200, 255);
				ImU32 pressButtonColor = IM_COL32(180, 180, 180, 255);

				bool clicked = false;

				//ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2.0f);

				if (disabled)
				{
					activeButtonColor = IM_COL32(255, 255, 255, 100);
					hoveredButtonColor = IM_COL32(255, 255, 255, 100);
					pressButtonColor = IM_COL32(255, 255, 255, 100);

					ImGui::ScopedStyle buttonColor(ImGuiCol_Button, ImVec4(0.05f, 0.05f, 0.05f, 0.54f));
					ImGui::ScopedStyle buttonColorActive(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.05f, 0.05f, 0.54f));
					ImGui::ScopedStyle buttonColorHovered(ImGuiCol_ButtonHovered, ImVec4(0.05f, 0.05f, 0.05f, 0.54f));

					ImGui::Button(labelId, ImVec2(iconSize, iconSize));
				}
				else
				{
					clicked = ImGui::Button(labelId, ImVec2(iconSize, iconSize));
				}


				ImRect result = { ImGui::GetItemRectMin(), ImGui::GetItemRectMax() };
				result.Min.x -= x;
				result.Min.y -= y;
				result.Max.x += x;
				result.Max.y += y;

				UserInterface::DrawButtonImage(icon,
					activeButtonColor,
					hoveredButtonColor,
					pressButtonColor,
					result
				);


				return clicked;
			};


			bool disableBackButton = false;
			if (m_CurrentDirectory->Handle == m_BaseDirectory->Handle)
				disableBackButton = true;

			if (contenBrowserButton("##ContentTopBar_BackButton", m_AssetIconMap["IconBack"], disableBackButton))
			{
				m_CurrentDirectory = m_CurrentDirectory->Parent;
				//RefreshContent();
				ChangeCurrentDirectory(m_CurrentDirectory);
			}

			ImGui::SameLine();
			contenBrowserButton("##ContentTopBar_ForwardButton", m_AssetIconMap["IconForward"]);

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetCursorPos().x + 3.0f);
			if (contenBrowserButton("##ContentTopBar_RefreshButton", m_AssetIconMap["IconRefresh"]))
			{
				RefreshContent();
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetCursorPos().x + 3.0f);

			
			SearchWidget(m_SearchBuffer);
			const float beforeCursourY = ImGui::GetCursorPosY();
			const float iconYOffset = 2.0f;

			Vector<Ref<DirectoryInfo>> directories;
			GetAllDirectoriesForBreadcrumbs(directories, m_CurrentDirectory);


			float offsetX = 360.0f;
			for (int32_t i = directories.size() - 1; i >= 0; i--)
			{
				RenderBreadcrumbs(directories, offsetX, beforeCursourY, i);
			}


		}
		ImGui::EndChild();
	}

	void ContentBrowserPanel::RenderBreadcrumbs(const Vector<Ref<DirectoryInfo>>& directories, float& offsetX, float cursorPosY, int32_t level)
	{
		if (level < 0) return;

		const Ref<DirectoryInfo>& directory = directories[level];
		
		{
			ImGui::ScopedFontStyle fontStyle(ImGui::ScopedFontStyle::FontType::Bold);

			ImGui::ScopedStyle buttonColor(ImGuiCol_Button, ImVec4(0.05f, 0.05f, 0.05f, 0.0f));

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			ImGui::SetCursorPosY(cursorPosY);

			if (!directory->IsRoot)
			{
				if (ImGui::Button(directory->FilePath.filename().string().c_str()))
				{
					m_CurrentDirectory = m_Directories[directory->Handle];
					//RefreshContent();
					ChangeCurrentDirectory(m_CurrentDirectory);
				}
			}
			else
			{
				if (ImGui::Button(Project::GetAssetDirectory().filename().string().c_str()))
				{
					m_CurrentDirectory = m_BaseDirectory;
					ChangeCurrentDirectory(m_CurrentDirectory);
					//RefreshContent();
				}
			}

			float buttonRectSize = ImGui::GetItemRectSize().x;
			float buttonSlashRectSize = 0.0f;

			{
				
				ImGui::SetCursorPosX(buttonRectSize + offsetX);
				ImGui::SetCursorPosY(cursorPosY);

				float defaultCursourPosY = ImGui::GetCursorPosY();

				ImGui::ScopedStyle buttonTransparentColor(ImGuiCol_Button, ImVec4(0.05f, 0.05f, 0.05f, 0.0f));
				ImGui::ScopedStyle buttonTransparentColorActive(ImGuiCol_ButtonActive, ImVec4(0.05f, 0.05f, 0.05f, 0.0f));
				ImGui::ScopedStyle buttonTransparentColorHovered(ImGuiCol_ButtonHovered, ImVec4(0.05f, 0.05f, 0.05f, 0.0f));


				ImGui::ScopedStyle buttonTextAlign(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));

				ImGui::Button("/");

				buttonSlashRectSize = ImGui::GetItemRectSize().x;

			}

			offsetX += buttonRectSize + buttonSlashRectSize;

		}
	}

	void ContentBrowserPanel::SearchWidget(char* searchString, const char* hint /*= "Search..."*/)
	{
		bool modified = false;
		bool searching = false;

		const float areaPosX = ImGui::GetCursorPosX();
		const float framePaddingY = ImGui::GetStyle().FramePadding.y;

		ImGui::ScopedStyle rounding(ImGuiStyleVar_FrameRounding, 3.0f);
		ImGui::ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(28.0f, framePaddingY + 2.0f));

		{
			ImGui::SetNextItemWidth(256.0f);

			const size_t buffSize = 128;

			if (ImGui::InputText("##SearchWidget", searchString, buffSize))
			{
				modified = true;
			}

			searching = m_SearchBuffer[0] != 0;
		}

		const float searchIconSize = 18.0f;
		ImGui::SameLine(areaPosX + 5.0f);
		const float beforeCursourX = ImGui::GetCursorPosX();
		const float beforeCursourY = ImGui::GetCursorPosY();

		// Search Icon
		{
			const float iconYOffset = 2.0f;
			UserInterface::ShiftCursorY(iconYOffset);

			ImGuiLayer* layer = Application::Get().GetImGuiLayer();
			ImTextureID texID = layer->GetImGuiTextureID(m_AssetIconMap["IconInputSearch"]->GetImage2D());
			ImGui::Image(texID, ImVec2(searchIconSize, searchIconSize), ImVec2(0, 0), ImVec2(1, 1));

			// Hint
			if (!searching)
			{
				ImGui::SetCursorPosY(beforeCursourY + iconYOffset + 3);
				ImGui::SetCursorPosX(beforeCursourX + searchIconSize + 8.0f);

				constexpr ImU32 textDarker = IM_COL32(128, 128, 128, 255);

				ImGui::ScopedStyle text(ImGuiCol_Text, textDarker);
				ImGui::TextUnformatted(hint);

			}
			ImGui::SetCursorPosY(beforeCursourY + iconYOffset);

		}
	}
}