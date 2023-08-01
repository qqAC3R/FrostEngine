#include "frostpch.h"
#include "AssetManager.h"

#include <json/nlohmann/json.hpp>
#include "Frost/Core/FunctionQueue.h"

#include "Frost/Project/Project.h"
#include "Frost/Utils/FileSystem.h"
#include "Frost/Asset/AssetExtensions.h"
#include "Frost/Renderer/MaterialAsset.h"
#include "Frost/Physics/PhysicsMaterial.h"

namespace Frost
{
	HashMap<AssetHandle, Ref<Asset>> AssetManager::s_LoadedAssets;

	void AssetManager::Init()
	{
		AssetImporter::Init();

		LoadAssetRegistry();
		//ReloadAssets();
	}

	static void DeleteAllAssetsByType(HashMap<AssetHandle, Ref<Asset>>& loadedAssets, AssetType type)
	{
		FunctionQueue lateFunction;

		for (auto& [handle, asset] : loadedAssets)
		{
			if (asset->GetAssetType() == type)
			{
				AssetHandle assetHandle = handle;
				lateFunction.AddFunction([&, assetHandle]()
				{
					loadedAssets.erase(assetHandle);
				});
			}
		}

		lateFunction.Execute();
	}

	void AssetManager::Shutdown()
	{
		WriteRegistryToFile();

		// NOTE:
		// In order to not have any errors while trying to clear all assets in one go, we have to delete them in a fashioned order (from big to small):
		// Firstly comes the biggest asset of all: Scene (why? because they own quite everything -> MeshAssets, etc... so thats why)
		// After that: Mesh Assets (why? they might own Materials and Textures, so they go second)
		// After that: Materials (why? because materials might contain some Textures)
		// After that: Textures and EnvMaps (here the order doesn't matter that much)
		DeleteAllAssetsByType(s_LoadedAssets, AssetType::Scene);
		DeleteAllAssetsByType(s_LoadedAssets, AssetType::MeshAsset);
		DeleteAllAssetsByType(s_LoadedAssets, AssetType::Material);
		//DeleteAllAssetsByType(s_LoadedAssets, AssetType::Texture); // Doesn't matter the order so much
		//DeleteAllAssetsByType(s_LoadedAssets, AssetType::EnvMap);  // Doesn't matter the order so much

		s_LoadedAssets.clear();
		s_AssetRegistry.Clear();
	}

	void AssetManager::LoadAssetRegistry()
	{
		FROST_CORE_INFO("[AssetManager] Loading Asset Registry");

		const auto& assetRegistryPath = Project::GetAssetRegistryPath();
		if (!FileSystem::Exists(assetRegistryPath))
			return;

		std::ifstream stream(Project::GetAssetRegistryFilePath());

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

	void AssetManager::OnRenameAsset(const std::filesystem::path& filepath, const std::string& name)
	{
		std::filesystem::path relativeFilepath = GetRelativePathString(filepath);
		if (s_AssetRegistry.Find(relativeFilepath))
		{
			std::filesystem::path result = "";

			for (auto file : relativeFilepath)
			{
				if (file.has_extension())
					result /= name;
				else
					result /= file.string();
			}

			s_AssetRegistry[result] = s_AssetRegistry[relativeFilepath];
			s_AssetRegistry[result].FilePath = result;

			// This is optional, and mostly required only for the assets that have a `Name` in their class
			switch (s_AssetRegistry[result].Type)
			{
				case AssetType::Material:
				{
					Ref<MaterialAsset> materialAsset = AssetManager::GetAsset<MaterialAsset>(s_AssetRegistry[result].FilePath.string());
					if (materialAsset)
						materialAsset->m_MaterialName = result.stem().string();
					break;
				}
				case AssetType::PhysicsMat:
				{
					Ref<PhysicsMaterial> phyiscsmatAsset = AssetManager::GetAsset<PhysicsMaterial>(s_AssetRegistry[result].FilePath.string());
					if (phyiscsmatAsset)
						phyiscsmatAsset->m_MaterialName = result.stem().string();
					break;
				}
			}

			s_AssetRegistry.Remove(relativeFilepath);
			WriteRegistryToFile();
		}
	}

	static bool is_subpath(const std::filesystem::path& path,
						   const std::filesystem::path& base)
	{
		auto rel = std::filesystem::relative(path, base);
		return !rel.empty() && rel.native()[0] != '.';
	}

	void AssetManager::OnRenameFilepath(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath)
	{
		FunctionQueue deleteFunction;

		std::filesystem::path oldFilepathRelative = GetRelativePathString(oldFilepath);
		std::filesystem::path newFilepathRelative = GetRelativePathString(newFilepath);

		for (auto& [filepath, assetMetaData] : s_AssetRegistry)
		{
			if (is_subpath(filepath, oldFilepathRelative))
			{
				std::filesystem::path assetFilePath = filepath;
				std::filesystem::path result = newFilepathRelative / filepath.filename();

				deleteFunction.AddFunction([&, assetFilePath, result]()
				{
					s_AssetRegistry[result] = s_AssetRegistry[assetFilePath];
					s_AssetRegistry[result].FilePath = result;

					s_AssetRegistry.Remove(assetFilePath);
				});

			}
		}

		deleteFunction.Execute();
	}


	void AssetManager::OnMoveFilepath(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath)
	{
		FunctionQueue deleteFunction;

		std::filesystem::path oldFilepathRelative = GetRelativePathString(oldFilepath);
		std::filesystem::path newFilepathRelative = GetRelativePathString(newFilepath);

		for (auto& [filepath, assetMetaData] : s_AssetRegistry)
		{
			if (is_subpath(filepath, oldFilepathRelative))
			{
				std::vector<std::string> oldFilePathVec;
				std::vector<std::string> newFilePathVec;

				for (auto file : filepath)
					oldFilePathVec.push_back(file.string());

				for (auto file : newFilepathRelative)
					newFilePathVec.push_back(file.string());


				//////////
				std::filesystem::path result = "";
				for (uint32_t i = 0; i < oldFilePathVec.size(); i++)
				{
					if (i >= newFilePathVec.size())
					{
						result /= oldFilePathVec[i];
						continue;
					}

					if (newFilePathVec[i] != oldFilePathVec[i])
						result /= newFilePathVec[i];
					else
						result /= oldFilePathVec[i];
				}

				std::filesystem::path assetFilePath = filepath;
				deleteFunction.AddFunction([&, assetFilePath, result]()
				{
					s_AssetRegistry[result] = s_AssetRegistry[assetFilePath];
					s_AssetRegistry[result].FilePath = result;

					s_AssetRegistry.Remove(assetFilePath);
				});

			}
		}

		deleteFunction.Execute();
	}

	void AssetManager::OnMoveAsset(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath)
	{
		std::filesystem::path relativeOldFilepath = GetRelativePathString(oldFilepath);
		std::filesystem::path relativeNewFlepath = GetRelativePathString(newFilepath);

		if (s_AssetRegistry.Find(relativeOldFilepath))
		{
			s_AssetRegistry[relativeNewFlepath] = s_AssetRegistry[relativeOldFilepath];
			s_AssetRegistry[relativeNewFlepath].FilePath = relativeNewFlepath;

			s_AssetRegistry.Remove(relativeOldFilepath);
		}
	}

	void AssetManager::OnAssetDeleted(AssetHandle assetHandle)
	{
		AssetMetadata metadata = GetMetadata(assetHandle);
		if (!metadata.IsValid())
			return;

		s_AssetRegistry.Remove(metadata.FilePath);
		s_LoadedAssets.erase(assetHandle);
		WriteRegistryToFile();
	}

	bool AssetManager::ReloadData(AssetHandle assetHandle)
	{
		auto& metadata = GetMetadataInternal(assetHandle);
		if (s_LoadedAssets.find(metadata.Handle) == s_LoadedAssets.end())
		{
			FROST_CORE_WARN("Trying to reload asset that was never loaded");

			Ref<Asset> asset;
			metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset, nullptr);
			return metadata.IsDataLoaded;
		}

		FROST_ASSERT_INTERNAL(bool(s_LoadedAssets.find(assetHandle) != s_LoadedAssets.end()));
		metadata.IsDataLoaded = s_LoadedAssets[assetHandle]->ReloadData(metadata.FilePath.string());

		return metadata.IsDataLoaded;
	}

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

	const AssetMetadata& AssetManager::GetMetadata(AssetHandle handle)
	{
		if (handle.Get() == 0) return AssetMetadata();

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
		if (temp.find(Project::GetAssetDirectory().string()) != std::string::npos)
		{
			return std::filesystem::relative(filepath, Project::GetAssetDirectory());
		}
		return filepath;
	}

	std::string AssetManager::GetRelativePathString(const std::filesystem::path& filepath)
	{
		std::string temp = GetRelativePath(filepath).string();
		std::replace(temp.begin(), temp.end(), '\\', '/');
		return temp;
	}
}