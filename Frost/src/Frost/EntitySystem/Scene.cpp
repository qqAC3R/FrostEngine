#include "frostpch.h"
#include "Scene.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/EntitySystem/Entity.h"
#include "Frost/EntitySystem/Components.h"

#include "Frost/Physics/PhysicsEngine.h"

#include "Frost/Math/Math.h"

namespace Frost
{

	Scene::~Scene()
	{

	}

	Entity Scene::CreateEntity(const std::string name)
	{
		// Creating the entity
		entt::entity entityHandle = m_Registry.create();
		Entity entity = Entity(entityHandle, this);

		// Adding the default components
		auto& idComponent = entity.AddComponent<IDComponent>();
		entity.AddComponent<ParentChildComponent>();
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();

		m_EntityIDMap[idComponent.ID] = entity;

		return entity;
	}

	Entity Scene::CreateEntityWithID(const UUID& id, const std::string name)
	{
		// Creating the entity
		entt::entity entityHandle = m_Registry.create();
		Entity entity = Entity(entityHandle, this);

		// Adding the default components
		entity.AddComponent<IDComponent>(id);
		entity.AddComponent<ParentChildComponent>();
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();

		m_EntityIDMap[id] = entity;

		return entity;
	}

	void Scene::ParentEntity(Entity& parent, Entity& entity)
	{
		ParentChildComponent& parentChildComponent = entity.GetComponent<ParentChildComponent>();

		// Case where the entity to be parented is the child of something else
		if (parent.IsChildOf(entity))
		{
			// Un-parent the parent first
			UnparentEntity(parent);

			Entity newParent = FindEntityByUUID(entity.GetParent());
			if (newParent)
			{
				UnparentEntity(entity);
				ParentEntity(parent, newParent);
			}
		}
		else
		{
			// Unparent if 'entity' was parented to something else before
			Entity previousParent = FindEntityByUUID(entity.GetParent());
			if (previousParent)
				UnparentEntity(entity);
		}

		parentChildComponent.ParentID = parent.GetUUID();
		parent.GetComponent<ParentChildComponent>().ChildIDs.push_back(entity.GetUUID());
		ConvertEntityToParentTransform(entity);
	}

	void Scene::Update(Timestep ts)
	{
		UpdateSkyLight(ts);
		UpdateMeshComponents(ts);
		UpdateAnimationControllers(ts);
		UpdatePointLightComponent(ts);
		UpdateDirectionalLight(ts);
		UpdateBoxFogVolumes(ts);
		UpdateCloudVolumes(ts);
	}

	void Scene::UpdateRuntime(Timestep ts)
	{
		PhysicsEngine::Simulate(ts);

		UpdateSkyLight(ts);
		UpdateMeshComponents(ts);
		UpdateAnimationControllers(ts);
		UpdatePointLightComponent(ts);
		UpdateDirectionalLight(ts);
		UpdateBoxFogVolumes(ts);
		UpdateCloudVolumes(ts);
	}

	void Scene::OnPhysicsSimulationStart()
	{
		PhysicsEngine::CreateScene();
		PhysicsEngine::CreateActors(this);
	}

	void Scene::OnPhysicsSimulationEnd()
	{
		PhysicsEngine::DeleteScene();
	}

	const glm::mat4& Scene::GetTransformMatFromEntityAndParent(Entity entity)
	{
		glm::mat4 transform(1.0f);

		Entity parent = FindEntityByUUID(entity.GetParent());
		if (parent)
			transform = GetTransformMatFromEntityAndParent(parent);

		return transform * entity.GetComponent<TransformComponent>().GetTransform();
	}

	void Scene::ConvertEntityToParentTransform(Entity entity)
	{
		Entity parent = FindEntityByUUID(entity.GetParent());

		if (!parent)
			return;

		TransformComponent& transform = entity.GetComponent<TransformComponent>();
		glm::mat4 parentTransform = GetTransformMatFromEntityAndParent(parent);

		// By doing inverse of the parent's matrix, we make the parent's position be the origin point for the child
		glm::mat4 localTransform = glm::inverse(parentTransform) * transform.GetTransform();
		Math::DecomposeTransform(localTransform, transform.Translation, transform.Rotation, transform.Scale);
	}

	Entity Scene::FindEntityByUUID(UUID id)
	{
		auto view = m_Registry.view<IDComponent>();
		for (const auto& entity : view)
		{
			auto& idComponent = m_Registry.get<IDComponent>(entity);
			if (idComponent.ID == id)
				return Entity(entity, this);
		}
		return Entity{};
	}

	CameraComponent* Scene::GetPrimaryCamera()
	{
		auto group = m_Registry.group<CameraComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [cameraComponent, transformComponent] = group.get<CameraComponent, TransformComponent>(entity);
			if (cameraComponent.Primary)
			{
				cameraComponent.Camera->SetTransform(transformComponent.GetTransform());
				return &cameraComponent;
			}
		}
		return nullptr;
	}

	void Scene::UpdateSkyLight(Timestep ts)
	{
		// Sky Light
		auto group = m_Registry.group<SkyLightComponent>(entt::get<IDComponent>);
		for (auto& entity : group)
		{
			auto [skyLightComponent, idComponent] = group.get<SkyLightComponent, IDComponent>(entity);
			if (skyLightComponent.IsActive)
			{
				Renderer::SetSky(skyLightComponent);
				return;
			}
		}

		Renderer::GetSceneEnvironment()->SetDynamicSky();
	}

	void Scene::UpdateMeshComponents(Timestep ts)
	{
		// Meshes
		auto group = m_Registry.group<MeshComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [mesh, transformComponent] = group.get<MeshComponent, TransformComponent>(entity);
			if (mesh.Mesh)
			{
				glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
				Renderer::Submit(mesh.Mesh, transform);
			}
		}
	}

	void Scene::UpdateAnimationControllers(Timestep ts)
	{
		// Animation Controllers
		auto group = m_Registry.group<AnimationComponent>(entt::get<MeshComponent>);
		for (auto& entity : group)
		{
			auto [animationController, mesh] = group.get<AnimationComponent, MeshComponent>(entity);
			if (mesh.Mesh)
			{
				animationController.Controller->OnUpdate(ts);
				mesh.Mesh->UpdateBoneTransformMatrices(animationController.Controller->GetModelSpaceMatrices());
			}
		}
	}

	void Scene::UpdatePointLightComponent(Timestep ts)
	{
		// Point lights
		auto group = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [pointLight, transformComponent] = group.get<PointLightComponent, TransformComponent>(entity);

			glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			glm::vec3 translation, temp;
			Math::DecomposeTransform(transform, translation, temp, temp);

			Renderer::Submit(pointLight, translation);
		}
	}

	void Scene::UpdateDirectionalLight(Timestep ts)
	{
		// Directional light
		auto group = m_Registry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [directionalLight, transformComponent] = group.get<DirectionalLightComponent, TransformComponent>(entity);

			Renderer::Submit(directionalLight, transformComponent.GetTransform()[2]);
		}
	}

	void Scene::UpdateBoxFogVolumes(Timestep ts)
	{
		// Box Fog Volumes
		auto group = m_Registry.group<FogBoxVolumeComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [fogVolumeComponent, transformComponent] = group.get<FogBoxVolumeComponent, TransformComponent>(entity);

			glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));

			Renderer::Submit(fogVolumeComponent, transform);
		}
	}

	void Scene::UpdateCloudVolumes(Timestep ts)
	{
		// Cloud Volumes
		auto group = m_Registry.group<CloudVolumeComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [cloudVolumeComponent, transformComponent] = group.get<CloudVolumeComponent, TransformComponent>(entity);

			glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			Renderer::Submit(cloudVolumeComponent, translation, scale);
		}
	}

	void Scene::DestroyEntity(Entity entity)
	{
		ParentChildComponent& pcc = entity.GetComponent<ParentChildComponent>();

		// Remove the current entity from the parent
		if (pcc.ParentID)
		{
			Entity parent = FindEntityByUUID(pcc.ParentID);
			Vector<UUID>& children = parent.GetComponent<ParentChildComponent>().ChildIDs;
			children.erase(std::remove(children.begin(), children.end(), entity.GetUUID()), children.end());
		}

		// Destroy all the child entities of the current entity, which will get destroyed
		Vector<UUID> children = pcc.ChildIDs;
		for (UUID childID : children)
		{
			Entity child = FindEntityByUUID(childID);
			if (child)
			{
				std::string childName = child.GetComponent<TagComponent>().Tag;
				pcc.ChildIDs.erase(std::remove(pcc.ChildIDs.begin(), pcc.ChildIDs.end(), child.GetUUID()), pcc.ChildIDs.end());
				DestroyEntity(child); // Destroy the childs recursively
			}
		}

		//UnparentEntity(entity);
		m_Registry.destroy(entity.Raw());
	}

	void Scene::UnparentEntity(Entity& child)
	{
		// Check if the entity has a valid parent
		Entity parent = FindEntityByUUID(child.GetParent());
		if (!parent)
			return;


		// Get the child UUIDs
		auto& children = parent.GetComponent<ParentChildComponent>().ChildIDs;

		// Remove the child from the children UUID list
		children.erase(std::remove(children.begin(), children.end(), child.GetUUID()), children.end());

		// Get the world position for the child (to make it indepedent on the parent)
		glm::mat4 nonParentTransform = GetTransformMatFromEntityAndParent(child);
		TransformComponent& childTransform = child.GetComponent<TransformComponent>();
		Math::DecomposeTransform(nonParentTransform, childTransform.Translation, childTransform.Rotation, childTransform.Scale);

		// Set the Parent to be 0, because we just unparented the entity
		child.GetComponent<ParentChildComponent>().ParentID = 0;
	}

	template<typename T>
	static void CopyComponent(entt::registry& dstRegistry, entt::registry& srcRegistry, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto components = srcRegistry.view<T>();
		for (auto srcEntity : components)
		{
			entt::entity destEntity = enttMap.at(srcRegistry.get<IDComponent>(srcEntity).ID);

			auto& srcComponent = srcRegistry.get<T>(srcEntity);
			auto& destComponent = dstRegistry.emplace_or_replace<T>(destEntity, srcComponent);
		}
	}

	void Scene::CopyTo(Ref<Scene>& target)
	{
		std::unordered_map<UUID, entt::entity> enttMap;
		auto idComponents = m_Registry.view<IDComponent>();
		for (auto entity : idComponents)
		{
			auto uuid = m_Registry.get<IDComponent>(entity).ID;
			auto name = m_Registry.get<TagComponent>(entity).Tag;
			Entity e = target->CreateEntityWithID(uuid, name);
			enttMap[uuid] = e.m_Handle;
		}


		CopyComponent<TagComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<TransformComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<MeshComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<AnimationComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CameraComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<PointLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<DirectionalLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<FogBoxVolumeComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CloudVolumeComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SkyLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<RigidBodyComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<BoxColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SphereColliderComponent>(target->m_Registry, m_Registry, enttMap);

		// Sort IdComponent by by entity handle (which is essentially the order in which they were created)
		// This ensures a consistent ordering when iterating IdComponent (for example: when rendering scene hierarchy panel)
		//target->m_Registry.sort<IDComponent>([&target](const auto lhs, const auto rhs)
		//{
		//	auto lhsEntity = target->GetEntityMap().find(lhs.ID);
		//	auto rhsEntity = target->GetEntityMap().find(rhs.ID);
		//	return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
		//});
	}

}