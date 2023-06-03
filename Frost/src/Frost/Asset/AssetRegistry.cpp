#include "frostpch.h"
#include "AssetRegistry.h"

#include "Frost/Project/Project.h"

namespace Frost
{
	static std::filesystem::path GetKey(const std::filesystem::path& path)
	{
		auto key = std::filesystem::relative(path, Project::GetAssetDirectory());
		if (key.empty())
			key = path.lexically_normal();

		return key;
	}

	AssetMetadata& AssetRegistry::operator[](const std::filesystem::path& path)
	{
		auto key = GetKey(path);
		FROST_ASSERT_INTERNAL(!path.string().empty());
		return m_AssetRegistry[key];
	}

	const AssetMetadata& AssetRegistry::Get(const std::filesystem::path& path) const
	{
		auto key = GetKey(path);
		FROST_ASSERT_INTERNAL(bool(m_AssetRegistry.find(key) != m_AssetRegistry.end()));
		FROST_ASSERT_INTERNAL(!path.string().empty());
		return m_AssetRegistry.at(key);

	}

	bool AssetRegistry::Contains(const std::filesystem::path& path) const
	{
		auto key = GetKey(path);
		return m_AssetRegistry.find(key) != m_AssetRegistry.end();
	}

	size_t AssetRegistry::Remove(const std::filesystem::path& path)
	{
		auto key = GetKey(path);
		return m_AssetRegistry.erase(key);
	}

	void AssetRegistry::Clear()
	{
		m_AssetRegistry.clear();
	}

}