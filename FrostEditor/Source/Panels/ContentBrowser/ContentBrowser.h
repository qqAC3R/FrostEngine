#pragma once

#include "Panels/Panel.h"
#include "ContentBrowserList.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"
#include "Panels/ContentBrowser/ContentBrowserSelectionStack.h"

#include <filesystem>

#define CONTENT_BROWSER_DRAG_DROP "content_browser_drag_drop"

namespace Frost
{
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

		//Vector<FileInfo> Files;
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

	private:
		static void InitAllAssetIcons();
		static void DeleteAllAssetIcons();
		static Ref<Texture2D> GetIconByAssetType(AssetType assetType);
		static Ref<Texture2D> GetFolderIcon();
	private:
		bool m_Visibility = true;
		
		static std::map<std::string, Ref<Texture2D>> m_AssetIconMap;

		ContentBrowserItemList m_CurrentItems;

		char m_SearchBuffer[128];
		//std::string m_SearchBuffer;

		Ref<DirectoryInfo> m_CurrentDirectory;
		Ref<DirectoryInfo> m_BaseDirectory;
		HashMap<UUID, Ref<DirectoryInfo>> m_Directories;
		
		SelectionStack m_SelectionStack;

		friend class ContentBrowserItem;
	};
}