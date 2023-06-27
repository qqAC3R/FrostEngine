#pragma once

#include "Panels/ContentBrowser/ContentBrowserItem.h"

namespace Frost
{
	struct ContentBrowserItemList
	{
		static constexpr size_t InvalidItem = std::numeric_limits<size_t>::max();

		Vector<Ref<ContentBrowserItem>> Items;

		Vector<Ref<ContentBrowserItem>>::iterator begin() { return Items.begin(); }
		Vector<Ref<ContentBrowserItem>>::iterator end() { return Items.end(); }
		Vector<Ref<ContentBrowserItem>>::const_iterator begin() const { return Items.begin(); }
		Vector<Ref<ContentBrowserItem>>::const_iterator end() const { return Items.end(); }

		Ref<ContentBrowserItem>& operator[](size_t index) { return Items[index]; }
		const Ref<ContentBrowserItem>& operator[](size_t index) const { return Items[index]; }

		void Clear()
		{
			Items.clear();
		}

		size_t FindItem(AssetHandle handle) const
		{
			for (size_t i = 0; i < Items.size(); i++)
			{
				if (Items[i]->GetID() == handle)
					return i;
			}

			return InvalidItem;
		}

#if 0
		void erase(AssetHandle handle)
		{
			size_t index = FindItem(handle);
			if (index == InvalidItem)
				return;

			auto it = Items.begin() + index;
			Items.erase(it);
		}

		
		}
#endif
	};
}