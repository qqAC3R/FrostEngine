#pragma once

#include "Frost/Asset/AssetTypes.h"

namespace Frost
{
	struct SelectionData
	{
		UUID Handle;
		ContentBrowserItem::ItemType ItemType;
		AssetType AssetType;
		std::filesystem::path FilePath;
	};

	class SelectionStack
	{
	public:
		SelectionStack()
		{
			m_Selections.reserve(64);
		}

		void CopyFrom(const SelectionStack& other)
		{
			m_Selections.assign(other.begin(), other.end());
		}

		void Select(UUID handle, ContentBrowserItem::ItemType itemType, AssetType assetType, const std::filesystem::path& filePath)
		{
			if (IsSelected(handle))
				return;

			m_Selections.push_back({ handle, itemType, assetType, filePath});
		}

		bool IsSelected(UUID handle) const
		{
			for (const auto& selectedHandle : m_Selections)
			{
				if (selectedHandle.Handle == handle)
					return true;
			}

			return false;
		}

		void Clear()
		{
			m_Selections.clear();
		}

		size_t SelectionCount() const { return m_Selections.size(); }
		const SelectionData* Data() const { return m_Selections.data(); }

		SelectionData operator[](size_t index) const
		{
			FROST_ASSERT_INTERNAL((index >= 0 && index < m_Selections.size()));
			return m_Selections[index];
		}

		Vector<SelectionData>::iterator begin() { return m_Selections.begin(); }
		Vector<SelectionData>::const_iterator begin() const { return m_Selections.begin(); }
		Vector<SelectionData>::iterator end() { return m_Selections.end(); }
		Vector<SelectionData>::const_iterator end() const { return m_Selections.end(); }

	private:
		Vector<SelectionData> m_Selections;
	};

}