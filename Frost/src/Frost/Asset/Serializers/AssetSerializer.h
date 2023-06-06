#pragma once

#include "Frost/Asset/AssetMetadata.h"

namespace Frost
{
	class AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const = 0;
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const = 0;
		virtual Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext) const = 0;
	};

	class MeshAssetSeralizer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const override {}
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const override;
		virtual Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext) const override { return nullptr; } // TODO:?
	};

	class MaterialAssetSerializer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const override;
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const override;
		virtual Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext) const override;
	};

	class TextureAssetSerializer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const override {}
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const override;
		virtual Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext) const override;
	};

}