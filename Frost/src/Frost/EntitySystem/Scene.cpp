#include "frostpch.h"
#include "Scene.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/EntitySystem/Entity.h"

namespace Frost
{

	Scene::~Scene()
	{

	}

	void Scene::Update()
	{
		{
			// Meshes
			auto group = m_Registry.group<MeshComponent>(entt::get<TransformComponent>);
			for (auto& entity : group)
			{
				auto [mesh, transformComponent] = group.get<MeshComponent, TransformComponent>(entity);
				if (mesh.Mesh)
				{
					Renderer::Submit(mesh.Mesh, transformComponent.GetTransform());
				}
			}
		}

		{
			// Point lights
			auto group = m_Registry.group<PointLightComponent>(entt::get<TransformComponent>);
			for (auto& entity : group)
			{
				auto [pointLight, transformComponent] = group.get<PointLightComponent, TransformComponent>(entity);
				Renderer::Submit(pointLight, transformComponent.Translation);
			}

		}

		{
			// Directional light
			auto group = m_Registry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
			for (auto& entity : group)
			{
				auto [directionalLight, transformComponent] = group.get<DirectionalLightComponent, TransformComponent>(entity);
				Renderer::Submit(directionalLight, transformComponent.GetTransform()[2]);
			}
		}

		{
			// Box Fog Volumes
			auto group = m_Registry.group<FogBoxVolumeComponent>(entt::get<TransformComponent>);
			for (auto& entity : group)
			{
				auto [fogVolumeComponent, transformComponent] = group.get<FogBoxVolumeComponent, TransformComponent>(entity);
				Renderer::Submit(fogVolumeComponent, transformComponent.GetTransform());
			}
		}
	}

	Entity Scene::CreateEntity(const std::string name)
	{
		// Creating the entity
		entt::entity entityHandle = m_Registry.create();
		Entity entity = Entity(entityHandle, this);

		// Adding the default components
		entity.AddComponent<IDComponent>();
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();

		return entity;
	}

	Entity Scene::CreateEntityWithID(const UUID& id, const std::string name)
	{
		// Creating the entity
		entt::entity entityHandle = m_Registry.create();
		Entity entity = Entity(entityHandle, this);

		// Adding the default components
		entity.AddComponent<IDComponent>(id);
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		m_Registry.destroy(entity.Raw());
	}

	Entity Scene::FindEntityByUUID(UUID id)
	{
		return Entity();
	}

}