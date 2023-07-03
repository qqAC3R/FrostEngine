#include "frostpch.h"
#include "PhysicsMaterial.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Asset/Serializers/AssetSerializer.h"

namespace Frost
{
	bool PhysicsMaterial::ReloadData(const std::string& filepath)
	{
		Ref<Asset> newPhysicsMaterialAsset;
		MaterialAssetSerializer::TryLoadDataStatic(AssetManager::GetMetadata(filepath), newPhysicsMaterialAsset, nullptr);
		CopyFrom(newPhysicsMaterialAsset.As<PhysicsMaterial>().Raw());
		return true;
	}
}