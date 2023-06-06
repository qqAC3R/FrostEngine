#pragma once

#include "Frost/Asset/Serializers/AssetSerializer.h"

namespace Frost
{

	class AssetImporter
	{
	public:
		static void Init();
		static void Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset);
		static void Serialize(const Ref<Asset>& asset);
#if 0
		template <typename T, typename... Args>
		static Ref<T> LoadData(const AssetMetadata& metadata, Args&&... args)
		{
			Ref<T> asset = CreateRefAsset<T>(metadata, std::forward<Args>(args)...);
			TryLoadData(metadata, asset.As<Asset>());
			return asset;
		}
#endif

		static bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext);
		static Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext);

	private:
#if 0
		template <typename T, typename... Args>
		static Ref<T> CreateRefAsset(const AssetMetadata& metadata, Args&&... args)
		{
			if (!std::is_base_of<Asset, T>::value)
				FROST_ASSERT_INTERNAL("CreateRefAsset only works for types derived from Asset");

			std::string filepath = AssetManager::GetFileSystemPathString(metadata);

			Ref<T> asset = nullptr;

			switch (T::GetStaticType())
			{
				case AssetType::MeshAsset: asset = MeshAsset::Load(filepath); break;
				case AssetType::Scene:	   asset = Ref<Scene>::Create(); break;
				case AssetType::Texture:   asset = Texture2D::Create(filepath, std::forward<Args>(args)...); ; break;
			}

			return asset;
		}
#endif

	private:
		static HashMap<AssetType, Scope<AssetSerializer>> s_Serializers;
	};

}