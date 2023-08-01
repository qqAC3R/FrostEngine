#pragma once

#include "Frost/Asset/Serializers/AssetSerializer.h"
#include "Frost/Renderer/Animation/AnimationBlueprint.h"

//#include <json/nlohmann/json.hpp>

namespace Frost
{

	class AnimationBlueprintSerializer : public AssetSerializer
	{
	public:
		virtual void Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const override;
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const override;
		virtual Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext) const override;

		static void SerializeBlueprint(const std::string& filepath, Ref<AnimationBlueprint> animationBlueprint);
		static bool DeserializeBlueprint(const std::string& filepath, Ref<AnimationBlueprint>& animationBlueprint);
	private:
		friend class PrefabSerializer;
		friend class Prefab;
	};

}