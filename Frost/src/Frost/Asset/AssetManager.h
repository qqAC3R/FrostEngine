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

		static std::filesystem::path GetFileSystemPath(const AssetMetadata& metadata) { return Project::GetAssetDirectory() / metadata.FilePath; }
		static std::string GetFileSystemPathString(const AssetMetadata& metadata) { return GetFileSystemPath(metadata).string(); }

		static bool IsAssetHandleValid(AssetHandle assetHandle) { GetMetadata(assetHandle).IsValid(); }
		static bool IsAssetLoaded(AssetHandle handle) { return s_LoadedAssets.find(handle) != s_LoadedAssets.end(); }

#if 0
		template<typename T>
		static Ref<T> GetAsset(const std::string& filepath)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("CreateNewAsset only works for types derived from Asset");

			std::string projectFilepath = AssetManager::GetRelativePath(filepath);
			AssetHandle assetHandle = GetAssetHandleFromFilePath(projectFilepath);

			if (IsAssetHandleValid(assetHandle))
			{
				if (IsAssetLoaded(assetHandle))
					return s_LoadedAssets.at(assetHandle).As<T>();

				AssetMetadata& metadata = GetMetadataInternal(assetHandle);

				Ref<Asset> asset = nullptr;
				metadata.IsDataLoaded = AssetImporter::TryLoadData(metadata, asset);

				if (!metadata.IsDataLoaded)
					return nullptr;

				s_LoadedAssets[assetHandle] = asset;
			}

			return nullptr;
		}
#endif

#if 0
		template<typename T, typename... Args>
		static Ref<T> CreateNewAsset(const std::string& filepath, Args&&... args)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("CreateNewAsset only works for types derived from Asset");

			AssetMetadata metadata;
			metadata.Handle = AssetHandle();
			if (filepath.empty() || filepath == ".")
				metadata.FilePath = filepath;
			else
				metadata.FilePath = AssetManager::GetRelativePath(filepath);
			metadata.IsDataLoaded = true;
			metadata.Type = T::GetStaticType();

			s_AssetRegistry[metadata.FilePath.string()] = metadata;

			WriteRegistryToFile();

			Ref<T> asset = Ref<T>::Create(std::forward<Args>(args)...);
			asset->Handle = metadata.Handle;
			s_MemoryAssets[asset->Handle] = asset;
			AssetImporter::Serialize(metadata, asset);
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

		//static bool ReloadData(AssetHandle assetHandle);

		static AssetMetadata& GetMetadataInternal(AssetHandle handle);

	private:
		static HashMap<AssetHandle, Ref<Asset>> s_LoadedAssets;
		inline static AssetRegistry s_AssetRegistry;
	};

}