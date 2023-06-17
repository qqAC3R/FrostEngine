#pragma once

#include "Frost/EntitySystem/Scene.h"
#include "Frost/Asset/Serializers/AssetSerializer.h"

#include <json/nlohmann/json.hpp>

namespace Frost
{
	class SceneSerializer : public AssetSerializer
	{
	public:
		//SceneSerializer(const Ref<Scene>& scene)
		
		virtual void Serialize(const AssetMetadata& metadata, Ref<Asset> asset) const override {}
		virtual bool TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const override;
		virtual Ref<Asset> CreateAssetRef(const AssetMetadata& metadata, void* pNext) const override { return nullptr; } // TODO:?

		static void SerializeScene(const std::string& filepath, Ref<Scene> scene);
		static void DeserializeScene(const std::string& filepath, Ref<Scene>& scene);

		//const std::string& GetSceneName() const { return m_SceneName; }

	private:
		static void SerializeEntity(nlohmann::ordered_json& out, Entity entity);
		static void DeserializeEntities(const std::string& filepath, Ref<Scene>& scene);

		friend class PrefabSerializer;
	};


}