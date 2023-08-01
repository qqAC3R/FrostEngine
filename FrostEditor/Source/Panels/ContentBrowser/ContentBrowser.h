#pragma once

#include "Panels/Panel.h"
#include "ContentBrowserList.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"
#include "Frost/Asset/AssetManager.h"
#include "Panels/ContentBrowser/ContentBrowserSelectionStack.h"

#include <filesystem>

#define CONTENT_BROWSER_DRAG_DROP "content_browser_drag_drop"

namespace Frost
{
	struct ContentBrowserDragDropData
	{
		// TODO: This technically causes memory leaks, because we dont actually delete the `Data` pointer
		ContentBrowserDragDropData(AssetType assetType, void* data, size_t sizeData)
		{
			AssetType = assetType;
			Data = new uint8_t[sizeData];
			memcpy(Data, data, sizeData);
		}

		AssetType AssetType;
		void* Data;
	};

	struct FileInfo
	{
		FileInfo(const UUID& handle, AssetType assetType, const std::filesystem::path& filepath)
			: Handle(handle), AssetType(assetType), FilePath(filepath) {}

		UUID Handle;
		AssetType AssetType;
		std::filesystem::path FilePath;
	};

	struct DirectoryInfo
	{
		UUID Handle;
		Ref<DirectoryInfo> Parent;

		std::filesystem::path FilePath;
		bool IsRoot = false;

		// This UUID is not related to the Asset Manager, only for Content Browser Internal purposes.
		// So the only way to get the AssetMetadata is through the filepath
		HashMap<UUID, Ref<FileInfo>> Files;
		std::map<std::filesystem::path, Ref<FileInfo>> ArrangedFiles;


		HashMap<UUID, Ref<DirectoryInfo>> SubDirectories;
		std::map<std::filesystem::path, Ref<DirectoryInfo>> ArrangedSubDirectories;
	};

	class ContentBrowserPanel : public Panel
	{
	public:
		ContentBrowserPanel();
		virtual ~ContentBrowserPanel();

		virtual void Init(void* initArgs) override;
		virtual void OnEvent(Event& e) override;
		virtual void Render() override;
		virtual void SetVisibility(bool show) override { m_Visibility = show; }
		virtual void Shutdown() override;

		bool IsContentBrowserFocused() const { return m_IsContentBrowserFocused; }

	private:
		void RenderDirectoryHierarchy(const Ref<DirectoryInfo>& directoryInfo, uint32_t level);

		void RenderItems();

		void RenderTopArea(float height);
		void RenderBreadcrumbs(const Vector<Ref<DirectoryInfo>>& directories, float& offsetX, float cursorPosY, int32_t level);
		void SearchWidget(char* searchString, const char* hint = "Search...");

		Ref<DirectoryInfo> GetDirectory(const std::filesystem::path& filepath) const;
		void ChangeCurrentDirectory(Ref<DirectoryInfo>& directory);

		void GetAllDirectoriesForBreadcrumbs(Vector<Ref<DirectoryInfo>>& directories, const Ref<DirectoryInfo>& directory);
		void RefreshAllDirectories();
		void RefreshContent();
		UUID ProcessDirectory(const std::filesystem::path& directoryPath, const Ref<DirectoryInfo>& parent);
	private:
		void ClearSelections();
		void PasteCopiedAssets();
		void PasteCutandPasteAssets();
		void RenderDeleteDialogue();

		bool OnKeyPressedEvent(KeyPressedEvent& e);

		template<typename T, typename... Args>
		Ref<T> CreateAsset(const std::string& filename, void* pNext = nullptr)
		{
			auto filePath = m_CurrentDirectory->FilePath / filename;
			Ref<T> asset = AssetManager::CreateNewAsset<T>(filePath.string(), pNext);
			if (!asset)
				return nullptr;

			filePath = AssetManager::GetFileSystemPathString(AssetManager::GetMetadata(filePath));

			UUID fileUUID = UUID();
			Ref<FileInfo> assetFileInfo = Ref<FileInfo>::Create(fileUUID, T::GetStaticType(), filePath);
			m_CurrentDirectory->Files[fileUUID] = assetFileInfo;
			m_CurrentDirectory->ArrangedFiles[filePath] = assetFileInfo;
			ChangeCurrentDirectory(m_CurrentDirectory);

			auto& item = m_CurrentItems[m_CurrentItems.FindItem(fileUUID)];
			m_SelectionStack.Select(fileUUID, ContentBrowserItem::ItemType::Asset, T::GetStaticType(), filePath);
			item->SetSelected(true);
			item->StartRenaming();

			return asset;
		}

		void UpdateDragDropTargetWithAssets();
		void UpdateInput();

	private:
		static void InitAllAssetIcons();
		static void DeleteAllAssetIcons();
		static Ref<Texture2D> GetIconByAssetType(AssetType assetType);
		static Ref<Texture2D> GetFolderIcon();
	private:
		bool m_Visibility = true;
		bool m_IsContentBrowserFocused = false;
		bool m_IsContentBrowserHovered = false;
		bool m_IsAnyItemHovered = false;

		bool m_OpenDeleteDialogue = false;
		bool m_IsDeleteDialogueOpened = false;
		
		static std::map<std::string, Ref<Texture2D>> m_AssetIconMap;

		ContentBrowserItemList m_CurrentItems;

		char m_SearchBuffer[128];

		Ref<DirectoryInfo> m_CurrentDirectory;
		Ref<DirectoryInfo> m_BaseDirectory;
		HashMap<UUID, Ref<DirectoryInfo>> m_Directories;
		
		SelectionStack m_SelectionStack;
		SelectionStack m_CopiedAssets;
		SelectionStack m_CutAndPasteAssets;

		friend class ContentBrowserItem;
	};
}