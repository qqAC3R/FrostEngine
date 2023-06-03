#include "frostpch.h"
#include "AssetImporter.h"

namespace Frost
{
	HashMap<AssetType, Scope<AssetSerializer>> AssetImporter::s_Serailizers;

	void AssetImporter::Init()
	{
		s_Serailizers[AssetType::MeshAsset] = CreateScope<MeshAssetSeralizer>();
	}

	void AssetImporter::Seralize(const AssetMetadata& metadata, const Ref<Asset>& asset)
	{

	}

	void AssetImporter::Seralize(const Ref<Asset>& asset)
	{

	}

	void AssetImporter::TryLoadData(const AssetMetadata& metadata, const Ref<Asset>& asset)
	{

	}


}