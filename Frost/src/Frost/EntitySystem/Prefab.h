#pragma once

#include "Frost/Asset/Asset.h"

#include "Entity.h"

namespace Frost
{
	class Prefab : public Asset
	{
	public:
		Prefab();
		~Prefab();

		void Create(Entity entity);

		static AssetType GetStaticType() { return AssetType::Prefab; }
		virtual AssetType GetAssetType() const override { return AssetType::Prefab; }

		virtual bool ReloadData(const std::string& filepath) override;

	private:
		Entity CreatePrefabFromEntity(Entity entity);
	private:
		Ref<Scene> m_Scene;
		Entity m_Entity;

		friend class Scene;
		friend class PrefabSerializer;
	};
}