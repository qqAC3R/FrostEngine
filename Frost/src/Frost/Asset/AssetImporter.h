#pragma once

#include "Frost/Asset/Serializers/AssetSerializer.h"

namespace Frost
{

	class AssetImporter
	{
	public:
		static void Init();
		static void Seralize(const AssetMetadata& metadata, const Ref<Asset>& asset);
		static void Seralize(const Ref<Asset>& asset);
		static void TryLoadData(const AssetMetadata& metadata, const Ref<Asset>& asset);

	private:
		static HashMap<AssetType, Scope<AssetSerializer>> s_Serailizers;
	};

}