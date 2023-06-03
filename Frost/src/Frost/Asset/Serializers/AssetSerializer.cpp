#include "frostpch.h"
#include "AssetSerializer.h"

#include "Frost/Asset/AssetManager.h"

namespace Frost
{

	bool MeshAssetSeralizer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset) const
	{
		//asset = Ref<MeshAsset>::Create(AssetManager::GetFileSystemPathString(metadata));
		asset->Handle = metadata.Handle;

		//return (asset.As<MeshAsset>())->GetVertices().size() > 0; // Maybe?
		return false;
	}

}