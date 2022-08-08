#include "frostpch.h"
#include "Scene.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/EntitySystem/Entity.h"

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
		entity.AddComponent<IDComponent>();
		entity.AddComponent<ParentChildComponent>();
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
		entity.AddComponent<ParentChildComponent>();
		entity.AddComponent<TagComponent>(name);
		entity.AddComponent<TransformComponent>();

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

					if(mesh.ActiveAnimation)
						mesh.Mesh->SetActiveAnimation(mesh.ActiveAnimation);
					mesh.Mesh->Update(ts);
				}
			}
		}

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

				glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));

				Renderer::Submit(fogVolumeComponent, transform);
			}
		}

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

}