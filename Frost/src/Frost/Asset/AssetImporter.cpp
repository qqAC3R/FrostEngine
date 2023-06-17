#include "frostpch.h"
#include "AssetImporter.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Asset/Serializers/SceneSerializer.h"

namespace Frost
{
	HashMap<AssetType, Scope<AssetSerializer>> AssetImporter::s_Serializers;

	void AssetImporter::Init()
	{
		s_Serializers[AssetType::MeshAsset] = CreateScope<MeshAssetSeralizer>();
		s_Serializers[AssetType::Scene] = CreateScope<SceneSerializer>();
		s_Serializers[AssetType::Material] = CreateScope<MaterialAssetSerializer>();
		s_Serializers[AssetType::Texture] = CreateScope<TextureAssetSerializer>();
		s_Serializers[AssetType::PhysicsMat] = CreateScope<PhysicsMaterialSerializer>();
		s_Serializers[AssetType::Prefab] = CreateScope<PrefabSerializer>();
	}

	void AssetImporter::Serialize(const AssetMetadata& metadata, const Ref<Asset>& asset)
	{
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			FROST_CORE_WARN("There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return;
		}

		s_Serializers[asset->GetAssetType()]->Serialize(metadata, asset);
	}

	void AssetImporter::Serialize(const Ref<Asset>& asset)
	{
		const AssetMetadata& metadata = AssetManager::GetMetadata(asset->Handle);
		Serialize(metadata, asset);
	}

	bool AssetImporter::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext)
	{
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			FROST_CORE_WARN("There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
			return false;
		}

		return s_Serializers[metadata.Type]->TryLoadData(metadata, asset, pNext);
	}

	Ref<Asset> AssetImporter::CreateAssetRef(const AssetMetadata& metadata, void* pNext)
	{
		if (s_Serializers.find(metadata.Type) == s_Serializers.end())
		{
			FROST_CORE_WARN("There's currently no importer for assets of type {0}", metadata.FilePath.stem().string());
		}

		return s_Serializers[metadata.Type]->CreateAssetRef(metadata, pNext);
	}

}