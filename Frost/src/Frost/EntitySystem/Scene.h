#pragma once

#include "Frost/Core/UUID.h"
#include "Frost/Renderer/EditorCamera.h"
#include <entt.hpp>

namespace Frost
{
	class Entity;
	struct CameraComponent;
	using EntityMap = HashMap<UUID, Entity>;

	class Scene
	{
	public:
		Scene() = default;
		virtual ~Scene();

		void Update(Timestep ts);
		void UpdateRuntime(Timestep ts);

		void OnPhysicsSimulationStart();
		void OnPhysicsSimulationEnd();

		Entity CreateEntity(const std::string name = "New Entity");
		Entity CreateEntityWithID(const UUID& id, const std::string name = "New Entity");
		void ParentEntity(Entity& parent, Entity& entity);
		
		void UnparentEntity(Entity& child);
		void DestroyEntity(Entity entity);

		const glm::mat4& GetTransformMatFromEntityAndParent(Entity entity);
		void ConvertEntityToParentTransform(Entity entity);

		Entity FindEntityByUUID(UUID id);

		entt::registry& GetRegistry() { return m_Registry; }
		const entt::registry& GetRegistry() const { return m_Registry; }

		CameraComponent* GetPrimaryCamera();

		const EntityMap& GetEntityMap() const { return m_EntityIDMap; }
		void CopyTo(Ref<Scene>& target);
	private:
		void UpdateSkyLight(Timestep ts);
		void UpdateMeshComponents(Timestep ts);
		void UpdateAnimationControllers(Timestep ts);
		void UpdatePointLightComponent(Timestep ts);
		void UpdateDirectionalLight(Timestep ts);
		void UpdateBoxFogVolumes(Timestep ts);
		void UpdateCloudVolumes(Timestep ts);
	private:
		entt::registry m_Registry;
		EntityMap m_EntityIDMap;

		friend class SceneSerializer;
		friend class PhysicsEngine;
	};
}