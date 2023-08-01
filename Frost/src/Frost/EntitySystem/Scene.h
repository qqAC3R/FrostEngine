#pragma once

#include "Frost/Asset/Asset.h"
#include "Frost/Core/UUID.h"
#include "Frost/Core/FunctionQueue.h"
#include "Frost/Renderer/EditorCamera.h"
#include <entt.hpp>

namespace Frost
{
	class Entity;
	class Prefab;
	struct CameraComponent;
	struct TransformComponent;
	using EntityMap = HashMap<UUID, Entity>;

	class Scene : public Asset
	{
	public:
		Scene(const std::string& name, bool construct = true); // Constructor which basically does nothing
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

		static AssetType GetStaticType() { return AssetType::Scene; }
		virtual AssetType GetAssetType() const override { return AssetType::Scene; }
		virtual bool ReloadData(const std::string& filepath) override;

		CameraComponent* GetPrimaryCamera();
		bool IsPlaying() const { return m_IsScenePlaying; }

		Entity CreatePrefabEntity(Entity entity, Entity parent, const glm::vec3* translation = nullptr);
		Entity Instantiate(Ref<Prefab> prefab, const glm::vec3* translation = nullptr);

		const EntityMap& GetEntityMap() const { return m_EntityIDMap; }
		void CopyTo(Ref<Scene>& target);

		void SetSelectedEntity(Entity entity);

		void ClearScene();

		template<typename T>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<T>();
		}
	public:
		static Ref<Scene> CreateEmpty();
	private:
		void UpdateSkyLight(Timestep ts);
		void UpdateMeshComponents(Timestep ts);
		void UpdateTextComponents(Timestep ts);
		void UpdateAnimationControllers(Timestep ts, bool isRuntime);
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
	private:
		void OnScriptComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnScriptComponentDestroy(entt::registry& registry, entt::entity entity);

		template<typename Fn>
		void SubmitPostUpdateFunc(Fn&& func)
		{
			m_PostUpdateQueue.AddFunction(func);
		}
	private:
		UUID m_SceneID;
		entt::registry m_Registry;
		entt::entity m_SelectedEntity = {};

		std::string m_Name;
		EntityMap m_EntityIDMap;

		bool m_IsScenePlaying = false;

		FunctionQueue m_PostUpdateQueue;

		friend class SceneSerializer;
		friend class EditorLayer;
		friend class PhysicsEngine;
		friend class RenderQueue;
		friend class Prefab;
		friend class PrefabSerializer;
	};
}