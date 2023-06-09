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

#if 0
				AssetMetadata& metadata = GetMetadataInternal(assetHandle);

				Ref<Asset> asset = nullptr;
				metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset);

				if (!metadata.IsDataLoaded)
					return nullptr;

				s_LoadedAssets[assetHandle] = asset;
				return asset.As<T>();
#endif
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


			if (!s_AssetRegistry.Find(metadata.FilePath))
			{
				s_AssetRegistry[metadata.FilePath.string()] = metadata;

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
				// This was mostly created for the MaterialAsset, because we firstly have to search for the data in files
				// If not found, create a new MaterialAsset
				// In the case of other assets, we try to just reload and thats it
				asset = AssetImporter::CreateAssetRef(metadata, pNext);

				if (asset)
					metadata.IsDataLoaded = true;
				else
					return nullptr;
			}

			asset->Handle = metadata.Handle;
			s_LoadedAssets[asset->Handle] = asset;


			if (!s_AssetRegistry.Find(metadata.FilePath))
			{
				s_AssetRegistry[metadata.FilePath.string()] = metadata;

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

#if 0
		static void CopyAssetToFilePath(Ref<Asset> asset, const std::string& newFilepath)
		{
			std::string newAssetFilepath = AssetManager::GetRelativePath(newFilepath).string();
			AssetMetadata oldMetadata = GetMetadataInternal(asset->Handle);

			AssetMetadata newMetadata;
			newMetadata.Handle = AssetHandle();
			newMetadata.FilePath = newAssetFilepath;
			newMetadata.Type = oldMetadata.Type;

			s_AssetRegistry[newMetadata.FilePath] = newMetadata;

			s_LoadedAssets[newMetadata.Handle] = s_LoadedAssets[oldMetadata.Handle];
		}
#endif
		

	public:

		static bool FileExists(AssetMetadata& metadata)
		{
			return FileSystem::Exists(Project::GetActive()->GetAssetDirectory() / metadata.FilePath);
		}

	private:
		static void LoadAssetRegistry();
		static void WriteRegistryToFile();

		static AssetMetadata& GetMetadataInternal(AssetHandle handle);

	private:
		static HashMap<AssetHandle, Ref<Asset>> s_LoadedAssets;
		inline static AssetRegistry s_AssetRegistry;
	};

}