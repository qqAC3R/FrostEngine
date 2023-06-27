#pragma once

#include "Frost/Renderer/Texture.h"
#include "Frost/Asset/AssetMetadata.h"

namespace Frost
{
	class ContentBrowserPanel;

	enum class ContentBrowserAction
	{
		None                = 0,
		Refresh             = BIT(0),
		Selected			= BIT(1), // When pressing Left Click
		ClearSelection		= BIT(2), // When pressing Left Click on another item and the selection stack must be cleared
		LeftShiftSelection	= BIT(3), // When pressing Shift + Left Click
		Hovered				= BIT(4),
		NavigateToThis		= BIT(5),
		ShowInExplorer		= BIT(6),
		OpenExternal		= BIT(7),
	};

	struct ContentBrowserItemAction
	{
		uint16_t Field = 0;

		void Set(ContentBrowserAction flag, bool value)
		{
			if (value)
				Field |= (uint16_t)flag;
			else
				Field &= ~(uint16_t)flag;
		}

		bool IsSet(ContentBrowserAction flag) const { return (uint16_t)flag & Field; }
	};

	class ContentBrowserItem
	{
	public:
		enum class ItemType : uint16_t
		{
			Directory, Asset
		};
	public:
		ContentBrowserItem(UUID itemID, ItemType type, const std::filesystem::path& filepath, ContentBrowserPanel* contentBrowserPanel);
		virtual ~ContentBrowserItem() {}

		void OnRenderBegin();
		ContentBrowserItemAction OnRender(float thumbnailSize, bool displayAssetType);
		void OnRenderEnd();

		void SetSelected(bool value);
		
		ItemType GetType() const { return m_Type; }
		const std::string& GetName() const { return m_Name; }
		const std::string& GetExtensionName() const { return m_Extension; }
		const std::filesystem::path& GetFilepath() const { return m_Filepath; }

		UUID GetID() const { return m_ID; }

		void StartRenaming();
		void StopRenaming();

	private:
		void OnContextMenuOpen(ContentBrowserItemAction& actionResult);

	private:
		ContentBrowserPanel* m_ContentBrowserPanel;
		UUID m_ID;

		ItemType m_Type;
		AssetType m_AssetType;
		std::filesystem::path m_Filepath;
		std::string m_Name;
		std::string m_Extension;
		//Ref<Texture2D> m_Icon;

		bool m_IsSelected = false;
		bool m_IsRenaming = false;
		bool m_IsDragging = false;

	private:
		friend class ContentBrowserPanel;
	};


}