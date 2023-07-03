#pragma once

#include "Frost/Core/UUID.h"
#include "Frost/Asset/AssetRegistry.h"
#include "Frost/Asset/AssetImporter.h"
#include "Frost/Project/Project.h"
#include "Frost/Utils/FileSystem.h"

#include <typeindex>

namespace Frost
{

	class AssetManager
	{
	public:
		static void Init();
		static void Shutdown();

		static const AssetMetadata& GetMetadata(AssetHandle handle);
		static const AssetMetadata& GetMetadata(const std::filesystem::path& filepath);
		static const AssetMetadata& GetMetadata(const Ref<Asset>& asset) { return GetMetadata(asset->Handle); }

		static AssetHandle GetAssetHandleFromFilePath(const std::filesystem::path& filepath);
		static std::filesystem::path GetRelativePath(const std::filesystem::path& filepath);
		static std::string GetRelativePathString(const std::filesystem::path& filepath);

		static std::filesystem::path GetFileSystemPath(const AssetMetadata& metadata) { return Project::GetAssetDirectory() / metadata.FilePath; }
		static std::string GetFileSystemPathString(const AssetMetadata& metadata) { return GetFileSystemPath(metadata).string(); }

		static bool IsAssetHandleValid(AssetHandle assetHandle) { return GetMetadata(assetHandle).IsValid(); }
		static bool IsAssetHandleNonZero(AssetHandle assetHandle) { return assetHandle.Get() != 0; }
		static bool IsAssetLoaded(AssetHandle handle) { return s_LoadedAssets.find(handle) != s_LoadedAssets.end(); }


		template<typename T>
		static Ref<T> GetAsset(const std::string& filepath)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("CreateNewAsset only works for types derived from Asset");

			std::filesystem::path projectFilepath = AssetManager::GetRelativePath(filepath);
			AssetHandle assetHandle = GetAssetHandleFromFilePath(projectFilepath);

			if (IsAssetHandleValid(assetHandle))
			{
				if (IsAssetLoaded(assetHandle))
					return s_LoadedAssets.at(assetHandle).As<T>();
			}

			return nullptr;
		}

		template<typename T>
		static Ref<T> LoadAsset(const std::string& filepath, void* pNext = nullptr)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("CreateNewAsset only works for types derived from Asset");

			AssetMetadata metadata;
			metadata.Handle = AssetHandle();
			if (filepath.empty() || filepath == ".")
				metadata.FilePath = filepath;
			else
				metadata.FilePath = AssetManager::GetRelativePath(filepath);

			if (s_AssetRegistry.Find(metadata.FilePath))
				metadata.Handle = GetMetadata(metadata.FilePath).Handle;

			metadata.Type = T::GetStaticType();

			Ref<Asset> asset = nullptr;
			metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset, pNext);

			if (!metadata.IsDataLoaded)
			{
				return nullptr;
			}

			asset->Handle = metadata.Handle;
			s_LoadedAssets[asset->Handle] = asset;
			s_AssetRegistry[metadata.FilePath.string()] = metadata;


			if (!s_AssetRegistry.Find(metadata.FilePath))
			{
				//s_AssetRegistry[metadata.FilePath.string()] = metadata;

				// If an asset has recently been created, we firstly serialize,
				// because it must have a physical location on the disk
				// (this is important when writting into the registry file because there we check which files are valid)
				if (metadata.IsDataLoaded)
				{
					AssetImporter::Serialize(s_LoadedAssets[asset->Handle]);
				}

				FROST_CORE_WARN("[AssetManager] (LoadAsset) with filepath: '{0}'", metadata.FilePath.string());
				WriteRegistryToFile();
			}

			return asset.As<T>();
		}

		template<typename T>
		static Ref<T> CreateNewAsset(const std::string& filepath, void* pNext = nullptr)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("CreateNewAsset only works for types derived from Asset");

			AssetMetadata metadata;
			metadata.Handle = AssetHandle();


			if (filepath.empty() || filepath == ".")
				metadata.FilePath = filepath;
			else
				metadata.FilePath = AssetManager::GetRelativePath(filepath);

			if (s_AssetRegistry.Find(metadata.FilePath))
				metadata.Handle = GetMetadata(metadata.FilePath).Handle;

			metadata.Type = T::GetStaticType();

			Ref<Asset> asset = nullptr;
			metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset, pNext);

			if (!metadata.IsDataLoaded)
			{
				// This was mostly designed for the MaterialAsset, Prefab, because we firstly have to search for the data in files
				// If not found, create a new MaterialAsset. In the case of other assets, we try to just load and thats it
				asset = AssetImporter::CreateAssetRef(metadata, pNext);

				if (asset)
					metadata.IsDataLoaded = true;
				else
					return nullptr;
			}

			asset->Handle = metadata.Handle;
			s_LoadedAssets[asset->Handle] = asset;
			s_AssetRegistry[metadata.FilePath.string()] = metadata;

			if (!s_AssetRegistry.Find(metadata.FilePath))
			{
				//s_AssetRegistry[metadata.FilePath.string()] = metadata;

				// If an asset has recently been created, we firstly serialize,
				// because it must have a physical location on the disk
				// (this is important when writting into the registry file because there we check which files are valid)
				if (metadata.IsDataLoaded)
				{
					AssetImporter::Serialize(s_LoadedAssets[asset->Handle]);
				}

				FROST_CORE_WARN("[AssetManager] (CreateNewAsset) with filepath: '{0}'", metadata.FilePath.string());
				WriteRegistryToFile();
			}

			return asset.As<T>();
		}

		template<typename T>
		static Ref<T> GetOrLoadAsset(const std::string& filepath, void* pNext = nullptr)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("GetOrCreateAsset only works for types derived from Asset");

			Ref<T> asset = GetAsset<T>(filepath);
			if (!asset)
				asset = LoadAsset<T>(filepath, pNext);
				//asset = CreateNewAsset<T>(filepath, pNext);

			return asset;
		}

		static void RemoveAssetFromMemory(AssetHandle assetHandle)
		{
			if (IsAssetHandleValid(assetHandle))
			{
				if (IsAssetLoaded(assetHandle))
				{
					AssetImporter::Serialize(s_LoadedAssets[assetHandle]);
					s_LoadedAssets.erase(assetHandle);
				}
				else
					FROST_CORE_ERROR("[AssetManager] Removing asset (UUID: {0}) which was not even loaded!", assetHandle.Get());
			}
			else
				FROST_CORE_ERROR("[AssetManager] Removing asset (UUID: {0}) which has an invalid handle!", assetHandle.Get());
		}

		static void RemoveAssetFromMemory(const std::string& filepath)
		{
			std::string assetFilepath = AssetManager::GetRelativePath(filepath).string();
			AssetHandle assetHandle = GetAssetHandleFromFilePath(assetFilepath);
			RemoveAssetFromMemory(assetHandle);
		}

		template<typename T>
		static Vector<Ref<T>> GetAllLoadedAssetsByType()
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("GetAllLoadedAssetsByType only works for types derived from Asset");

			Vector<Ref<T>> assets;
			for (auto& [filepath, metadata] : s_AssetRegistry)
			{
				if (metadata.Type == T::GetStaticType())
					assets.push_back(s_LoadedAssets[metadata.Handle]);
			}
			return assets;
		}
	public:
		static bool FileExists(AssetMetadata& metadata)
		{
			return FileSystem::Exists(Project::GetActive()->GetAssetDirectory() / metadata.FilePath);
		}

		static bool ReloadData(AssetHandle assetHandle);

		static void OnMoveAsset(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static void OnRenameAsset(const std::filesystem::path& filepath, const std::string& name);
		static void OnAssetDeleted(AssetHandle assetHandle);

		static void OnMoveFilepath(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static void OnRenameFilepath(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);

	private:
		static void LoadAssetRegistry();
		static void WriteRegistryToFile();

		static AssetMetadata& GetMetadataInternal(AssetHandle handle);

	private:
		static HashMap<AssetHandle, Ref<Asset>> s_LoadedAssets;
		inline static AssetRegistry s_AssetRegistry;
	};

}