#include "frostpch.h"
#include "Prefab.h"

#include "Frost/Asset/AssetImporter.h"

namespace Frost
{
	template<typename T>
	static void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src, entt::registry& srcRegistry)
	{
		if (srcRegistry.any_of<T>(src))
		{
			auto& srcComponent = srcRegistry.get<T>(src);
			dstRegistry.emplace_or_replace<T>(dst, srcComponent);
		}
	}

	Prefab::Prefab()
	{
		m_Scene = Scene::CreateEmpty();
		m_Scene->m_Registry.on_construct<ScriptComponent>().connect<&Scene::OnScriptComponentConstruct>(m_Scene);
		m_Scene->m_Registry.on_destroy<ScriptComponent>().connect<&Scene::OnScriptComponentDestroy>(m_Scene);
	}

	void Prefab::Create(Entity entity)
	{
		m_Scene = Scene::CreateEmpty();
		m_Entity = CreatePrefabFromEntity(entity);
	}

	Entity Prefab::CreatePrefabFromEntity(Entity entity)
	{
		FROST_ASSERT_INTERNAL(Handle);

		Entity newEntity = m_Scene->CreateEntity();

		// Add PrefabComponent
		newEntity.AddComponent<PrefabComponent>(Handle);

		CopyComponentIfExists<TagComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<TransformComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<MeshComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<AnimationComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<CameraComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<PointLightComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<DirectionalLightComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<SkyLightComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<RectangularLightComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<FogBoxVolumeComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<CloudVolumeComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<RigidBodyComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<BoxColliderComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<SphereColliderComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<CapsuleColliderComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<MeshColliderComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<ScriptComponent>(newEntity, m_Scene->m_Registry, entity, entity.m_Scene->m_Registry);

		for (auto childId : entity.GetChildren())
		{
			Entity childDuplicate = CreatePrefabFromEntity(entity.m_Scene->FindEntityByUUID(childId));

			childDuplicate.SetParentUUID(newEntity.GetUUID());
			newEntity.GetChildren().push_back(childDuplicate.GetUUID());
		}

		return newEntity;
	}

	Prefab::~Prefab()
	{
		m_Scene->m_Registry.on_construct<ScriptComponent>().disconnect(this);
		m_Scene->m_Registry.on_destroy<ScriptComponent>().disconnect(this);
	}
}