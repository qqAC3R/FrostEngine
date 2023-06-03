#pragma once

#include "Frost/Asset/AssetMetadata.h"

#include <unordered_map>
#include <filesystem>

namespace Frost
{

	class AssetRegistry
	{
	public:
		AssetMetadata& operator[](const std::filesystem::path& path);
		const AssetMetadata& Get(const std::filesystem::path& path) const;

		size_t Count() const { return m_AssetRegistry.size(); }

		bool Contains(const std::filesystem::path& path) const;
		size_t Remove(const std::filesystem::path& path);
		void Clear();

		// Iterators
		HashMap<std::filesystem::path, AssetMetadata>::iterator begin() { return m_AssetRegistry.begin(); }
		HashMap<std::filesystem::path, AssetMetadata>::iterator end() { return m_AssetRegistry.end(); }

		// Const Iterators
		HashMap<std::filesystem::path, AssetMetadata>::const_iterator cbegin() const { return m_AssetRegistry.cbegin(); }
		HashMap<std::filesystem::path, AssetMetadata>::const_iterator cend() const { return m_AssetRegistry.cend(); }

	private:
		HashMap<std::filesystem::path, AssetMetadata> m_AssetRegistry;
	};

}