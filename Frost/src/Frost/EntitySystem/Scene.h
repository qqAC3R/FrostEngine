#pragma once

#include "Frost/Core/UUID.h"
#include "Frost/Renderer/EditorCamera.h"
#include <entt.hpp>

namespace Frost
{
	class Entity;
	struct CameraComponent;
	struct TransformComponent;
	using EntityMap = HashMap<UUID, Entity>;

	class Scene
	{
	public:
		Scene();
		virtual ~Scene();

		void Update(Timestep ts);
		void UpdateRuntime(Timestep ts);

		void OnRuntimeStart();
		void OnRuntimeEnd();

		Entity CreateEntity(const std::string name = "New Entity");
		Entity CreateEntityWithID(const UUID& id, const std::string name = "New Entity");
		void ParentEntity(Entity& parent, Entity& entity);
		
		void UnparentEntity(Entity& child);
		void SubmitToDestroyEntity(Entity entity);
		void DestroyEntity(Entity entity);

		void ConvertEntityToParentTransform(Entity entity);
		const glm::mat4& GetTransformMatFromEntityAndParent(Entity entity);
		TransformComponent GetTransformFromEntityAndParent(Entity entity);

		Entity FindEntityByUUID(UUID id);
		Entity FindEntityByTag(const std::string& tag);

		entt::registry& GetRegistry() { return m_Registry; }
		const entt::registry& GetRegistry() const { return m_Registry; }
		UUID GetUUID() const { return m_SceneID; }

		CameraComponent* GetPrimaryCamera();
		bool IsPlaying() const { return m_IsScenePlaying; }

		Entity GetEntityByUUID(UUID uuid);
		const EntityMap& GetEntityMap() const { return m_EntityIDMap; }
		void CopyTo(Ref<Scene>& target);
	private:
		void UpdateSkyLight(Timestep ts);
		void UpdateMeshComponents(Timestep ts);
		void UpdateAnimationControllers(Timestep ts);
		void UpdatePointLightComponent(Timestep ts);
		void UpdateRectangularLightComponent(Timestep ts);
		void UpdateDirectionalLight(Timestep ts);
		void UpdateBoxFogVolumes(Timestep ts);
		void UpdateCloudVolumes(Timestep ts);
		// This is only for editor mode (debug), for rendering billboards/wireframe meshes (in play mode we shouldn't see camera icon)
		void UpdateSceneCameras(Timestep ts);
		void UpdatePhysicsDebugMesh(Entity selectedEntity);
		void UpdatePhysicsDebugMeshes(Timestep ts);
		void UpdateCSharpApplication(Timestep ts);

		void UpdateChildrenTransforms(Timestep ts);

		template<typename Fn>
		void SubmitPostUpdateFunc(Fn&& func)
		{
			m_PostUpdateQueue.emplace_back(func);
		}
	private:
		UUID m_SceneID;
		//entt::entity m_SceneEntity;
		entt::registry m_Registry;
		EntityMap m_EntityIDMap;

		std::vector<std::function<void()>> m_PostUpdateQueue;

		bool m_IsScenePlaying = false;

		friend class SceneSerializer;
		friend class EditorLayer;
		friend class PhysicsEngine;
	};
}