#include "frostpch.h"
#include "AssetManager.h"

#include <json/nlohmann/json.hpp>

#include "Frost/Project/Project.h"
#include "Frost/Utils/FileSystem.h"
#include "Frost/Asset/AssetExtensions.h"

namespace Frost
{
	HashMap<AssetHandle, Ref<Asset>> AssetManager::s_LoadedAssets;

	void AssetManager::Init()
	{
		AssetImporter::Init();

		LoadAssetRegistry();
		//ReloadAssets();
	}

	void AssetManager::Shutdown()
	{
		WriteRegistryToFile();

		s_LoadedAssets.clear();
		s_AssetRegistry.Clear();
	}

	void AssetManager::LoadAssetRegistry()
	{
		FROST_CORE_INFO("[AssetManager] Loading Asset Registry");

		const auto& assetRegistryPath = Project::GetAssetRegistryPath();
		if (!FileSystem::Exists(assetRegistryPath))
			return;

		std::ifstream stream(assetRegistryPath);

		if (!stream.is_open())
		{
			// If we have not found a asset registry file, then create one
			stream.close();

			WriteRegistryToFile();

			FROST_CORE_INFO("[AssetManager] Registry Asset File not found! Creating new one...");
			return;
		}

		std::stringstream strStream;
		strStream << stream.rdbuf();

		nlohmann::ordered_json data = nlohmann::json::parse(strStream.str());
		
		for (auto asset : data)
		{
			std::string filepath = asset["FilePath"];

			AssetMetadata metadata;
			metadata.Handle = UUID(asset["Handle"]);
			metadata.FilePath = filepath;
			metadata.Type = (AssetType)Utils::AssetTypeFromString(asset["Type"]);

			if(metadata.Type == AssetType::None)
				continue;

			if (!FileSystem::Exists(AssetManager::GetFileSystemPath(metadata)))
			{
				// Search the whole project
			}

			if (metadata.Handle == 0)
			{
				FROST_CORE_WARN("[AssetManager] AssetHandle for {0} is 0, this shouldn't happen.", metadata.FilePath.string());
				continue;
			}

			s_AssetRegistry[metadata.FilePath] = metadata;
		}

		FROST_CORE_INFO("[AssetManager] Loaded {0} asset entries", s_AssetRegistry.Count());

		/*
		[
			{
				"Handle": 12312413432523,
				"FilePath": "Mesh/Cube.fbx",
				"Type": "MeshAsset",
			},

			{
				"Handle": 46238946723452,
				"FilePath": "Materials/BlueMat.fmat",
				"Type": "Material",
			},
		]
		*/
	}

	void AssetManager::WriteRegistryToFile()
	{
		// Sort assets by UUID to make project managment easier
		struct AssetRegistryEntry
		{
			std::string FilePath;
			AssetType Type;
		};
		std::map<UUID, AssetRegistryEntry> sortedMap;
		for (auto& [filepath, metadata] : s_AssetRegistry)
		{
			if(!FileSystem::Exists(AssetManager::GetFileSystemPath(metadata)))
				continue;

			std::string pathToSerialize = metadata.FilePath.string();
			std::replace(pathToSerialize.begin(), pathToSerialize.end(), '\\', '/');
			sortedMap[metadata.Handle] = { pathToSerialize, metadata.Type };
		}

		FROST_CORE_INFO("[AssetManager] Serializing asset registry with {0} entries", sortedMap.size());

		nlohmann::ordered_json out = nlohmann::ordered_json();
		for (auto& [handle, entry] : sortedMap)
		{
			nlohmann::ordered_json assetJson = nlohmann::ordered_json();
			assetJson["Handle"] = handle.Get();
			assetJson["FilePath"] = entry.FilePath;
			assetJson["Type"] = Utils::AssetTypeToString(entry.Type);

			out.push_back(assetJson);
		}

		const std::string& assetRegistryPath = Project::GetAssetRegistryFilePath().string();
		std::ofstream fout(assetRegistryPath);
		fout << out.dump(4);
	}

#if 0
	bool AssetManager::ReloadData(AssetHandle assetHandle)
	{
		auto& metadata = GetMetadataInternal(assetHandle);
		if (!metadata.IsDataLoaded)
		{
			FROST_CORE_WARN("Trying to reload asset that was never loaded");

			Ref<Asset> asset;
			metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset);
			return metadata.IsDataLoaded;
		}

		FROST_ASSERT_INTERNAL(bool(s_LoadedAssets.find(assetHandle) != s_LoadedAssets.end()));
		Ref<Asset>& asset = s_LoadedAssets.at(assetHandle);
		metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset);
		return metadata.IsDataLoaded;
	}
#endif

	static AssetMetadata s_NullMetadata;
	AssetMetadata& AssetManager::GetMetadataInternal(AssetHandle handle)
	{
		for (auto& [filepath, metadata] : s_AssetRegistry)
		{
			if (metadata.Handle == handle)
				return metadata;
		}

		return s_NullMetadata;
	}

	const Frost::AssetMetadata& AssetManager::GetMetadata(AssetHandle handle)
	{
		return GetMetadataInternal(handle);
	}

	const Frost::AssetMetadata& AssetManager::GetMetadata(const std::filesystem::path& filepath)
	{
		if (s_AssetRegistry.Contains(filepath))
			return s_AssetRegistry[filepath];

		return s_NullMetadata;
	}

	AssetHandle AssetManager::GetAssetHandleFromFilePath(const std::filesystem::path& filepath)
	{
		return s_AssetRegistry.Contains(filepath) ? s_AssetRegistry[filepath].Handle : 0;
	}

	std::filesystem::path AssetManager::GetRelativePath(const std::filesystem::path& filepath)
	{
		std::string temp = filepath.string();
		if (temp.find(Project::GetActive()->GetAssetDirectory().string()) != std::string::npos)
			return std::filesystem::relative(filepath, Project::GetActive()->GetAssetDirectory());
		return filepath;
	}

}