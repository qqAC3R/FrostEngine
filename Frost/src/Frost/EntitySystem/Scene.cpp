#include "frostpch.h"
#include "Scene.h"

#include "Frost/Asset/AssetManager.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/EntitySystem/Entity.h"
#include "Frost/EntitySystem/Components.h"
#include "Frost/EntitySystem/Prefab.h"

#include "Frost/Physics/PhysicsEngine.h"
#include "Frost/Script/ScriptEngine.h"

#include "Frost/Math/Math.h"

namespace Frost
{

	Scene::Scene(const std::string& name, bool construct)
		: m_Name(name)
	{
		if (construct)
		{
			m_Registry.on_construct<ScriptComponent>().connect<&Scene::OnScriptComponentConstruct>(this);
			m_Registry.on_destroy<ScriptComponent>().connect<&Scene::OnScriptComponentDestroy>(this);
		}
	}

	Scene::~Scene()
	{
		m_Registry.on_destroy<ScriptComponent>().disconnect();
#if 0
		m_Registry.each([&](auto entity)
		{
			Entity ent = { entity, this };
			if(ent.HasComponent<MeshComponent>())
			{
				MeshComponent& meshComponent = ent.GetComponent<MeshComponent>();
				if (meshComponent.IsMeshValid())
				{
					AssetHandle assetHandle = meshComponent.Mesh->GetMeshAsset()->Handle;
					meshComponent.Mesh = nullptr;
					AssetManager::RemoveAssetFromMemory(assetHandle);
				}
			}
		});
#endif
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
		UpdateRectangularLightComponent(ts);
		UpdateBoxFogVolumes(ts);
		UpdateCloudVolumes(ts);

		UpdateTextComponents(ts);
		UpdateSceneCameras(ts);


		//UpdatePhysicsDebugMeshes(ts);
	}

	void Scene::UpdateRuntime(Timestep ts)
	{
		// C# Scripting Part
		UpdateCSharpApplication(ts);

		// Physics Part
		PhysicsEngine::Simulate(ts);

		// Rendering Part
		UpdateSkyLight(ts);
		UpdateMeshComponents(ts);
		UpdateAnimationControllers(ts);
		UpdatePointLightComponent(ts);
		UpdateDirectionalLight(ts);
		UpdateRectangularLightComponent(ts);
		UpdateBoxFogVolumes(ts);
		UpdateCloudVolumes(ts);

		UpdateTextComponents(ts);
		//UpdatePhysicsDebugMeshes(ts);
	}

	void Scene::OnRuntimeStart()
	{
		FROST_CORE_TRACE("==========RunTimeSTART==========");

		m_IsScenePlaying = true;

		// Initalizing the physics engine firstly
		PhysicsEngine::CreateScene();
		PhysicsEngine::CreateActors(this);


		// Initializing the script engine
		ScriptEngine::SetSceneContext(this);
		auto view = m_Registry.view<ScriptComponent>();
		for (auto entity : view)
		{
			Entity e = { entity, this };
			if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
			{
				ScriptEngine::InitScriptEntity(e);
				ScriptEngine::InstantiateEntityClass(e);
			}
		}

		for (auto entity : view)
		{
			Entity e = { entity, this };
			if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
			{
				ScriptEngine::OnCreateEntity(e);
			}
		}
	}

	void Scene::OnRuntimeEnd()
	{
		PhysicsEngine::DeleteScene();

		auto view = m_Registry.view<ScriptComponent>();
		for (auto entity : view)
		{
			Entity e = { entity, this };
			if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
			{
				ScriptEngine::OnDestroyEntity(e);
				//ScriptEngine::ShutdownScriptEntity(e, e.GetComponent<ScriptComponent>().ModuleName);
			}
		}

		ScriptEngine::ShutdownScene(this);

		m_IsScenePlaying = false;

		FROST_CORE_TRACE("==========RunTimeEND==========");
	}

	const glm::mat4& Scene::GetTransformMatFromEntityAndParent(Entity entity)
	{
		glm::mat4 transform(1.0f);

		Entity parent = FindEntityByUUID(entity.GetParent());
		if (parent)
			transform = GetTransformMatFromEntityAndParent(parent);

		return transform * entity.GetComponent<TransformComponent>().GetTransform();
	}

	TransformComponent Scene::GetTransformFromEntityAndParent(Entity entity)
	{
		glm::mat4 transform = GetTransformMatFromEntityAndParent(entity);
		TransformComponent transformComponent;
		Math::DecomposeTransform(transform, transformComponent.Translation, transformComponent.Rotation, transformComponent.Scale);

		return transformComponent;
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
		if (m_EntityIDMap.find(id) != m_EntityIDMap.end())
		{
			return m_EntityIDMap[id];
		}
		return Entity{};

		//auto view = m_Registry.view<IDComponent>();
		//for (const auto& entity : view)
		//{
		//	auto& idComponent = m_Registry.get<IDComponent>(entity);
		//	if (idComponent.ID == id)
		//		return Entity(entity, this);
		//}
	}

	Entity Scene::FindEntityByTag(const std::string& tag)
	{
		auto view = m_Registry.view<TagComponent>();
		for (auto entity : view)
		{
			const auto& tagComponent = view.get<TagComponent>(entity).Tag;
			if (tagComponent == tag)
				return Entity(entity, this);
		}
		return Entity{};
	}

	bool Scene::ReloadData(const std::string& filepath)
	{
		ClearScene();
		//SceneSerializer::DeserializeScene(filepath, )
		return false;
	}

	CameraComponent* Scene::GetPrimaryCamera()
	{
		auto group = m_Registry.group<CameraComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [cameraComponent, transformComponent] = group.get<CameraComponent, TransformComponent>(entity);
			if (cameraComponent.Primary)
			{
				glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
				cameraComponent.Camera->SetTransform(transform);
				return &cameraComponent;
			}
		}
		return nullptr;
	}

	template<typename T>
	static void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src, entt::registry& srcRegistry)
	{
		if (srcRegistry.any_of<T>(src))
		{
			auto& srcComponent = srcRegistry.get<T>(src);
			dstRegistry.emplace_or_replace<T>(dst, srcComponent);
		}
	}
	
	template<typename T>
	static void CopyComponentIfExists(entt::entity dst, entt::entity src, entt::registry& registry)
	{
		if (registry.any_of<T>(src))
		{
			auto& srcComponent = registry.get<T>(src);
			registry.emplace_or_replace<T>(dst, srcComponent);
		}
	}

	Entity Scene::CreatePrefabEntity(Entity entity, Entity parent, const glm::vec3* translation)
	{
		FROST_ASSERT_INTERNAL(entity.HasComponent<PrefabComponent>());

		Entity newEntity = CreateEntity();
		if (parent)
		{
			newEntity.SetParentUUID(parent.GetComponent<IDComponent>().ID);
		}
		else
		{
			PrefabComponent& prefabComponent = newEntity.AddComponent<PrefabComponent>();
			prefabComponent.PrefabAssetHandle = entity.GetComponent<PrefabComponent>().PrefabAssetHandle;
			//prefabComponent.EntityID = newEntity.GetUUID();
		}

		CopyComponentIfExists<TagComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<TransformComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<MeshComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<AnimationComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<CameraComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<PointLightComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<DirectionalLightComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<SkyLightComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<RectangularLightComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<FogBoxVolumeComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<CloudVolumeComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<RigidBodyComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<BoxColliderComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<SphereColliderComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<CapsuleColliderComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<MeshColliderComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);
		CopyComponentIfExists<ScriptComponent>(newEntity, m_Registry, entity, entity.m_Scene->m_Registry);

		
		if (translation)
			newEntity.Transform().Translation = *translation;

		// Create children
		for (auto childId : entity.GetChildren())
		{
			Entity child = CreatePrefabEntity(entity.m_Scene->FindEntityByUUID(childId), newEntity);
			newEntity.GetChildren().push_back(child.GetUUID());
		}

		if (m_IsScenePlaying)
		{
			if (newEntity.HasComponent<RigidBodyComponent>())
			{
				Ref<PhysicsScene> physicsScene = PhysicsEngine::GetScene();
				if (physicsScene)
				{
					physicsScene->CreateActor(newEntity);
				}

				if (newEntity.HasComponent<ScriptComponent>())
				{
					if (ScriptEngine::ModuleExists(newEntity.GetComponent<ScriptComponent>().ModuleName))
					{
						ScriptEngine::InstantiateEntityClass(newEntity);
						ScriptEngine::OnCreateEntity(newEntity);
					}
				}
			}

		}

		return newEntity;
	}

	Entity Scene::Instantiate(Ref<Prefab> prefab, const glm::vec3* translation)
	{
		Entity result;

		auto entities = prefab->m_Scene->GetAllEntitiesWith<ParentChildComponent>();
		for (auto e : entities)
		{
			Entity entity = { e, prefab->m_Scene.Raw() };
			if (!entity.HasParent())
			{
				result = CreatePrefabEntity(entity, {}, translation);
				break;
			}
		}

		return result;
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
				Renderer::Submit(mesh.Mesh, transform, (uint32_t)entity);
			}
		}
	}

	void Scene::UpdateTextComponents(Timestep ts)
	{
		// Meshes
		auto group = m_Registry.group<TextComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [textComponent, transformComponent] = group.get<TextComponent, TransformComponent>(entity);
			if (!textComponent.TextString.empty())
			{
				glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));

				Renderer::SubmitText(
					textComponent.TextString,
					textComponent.FontAsset,
					transform,
					textComponent.MaxWidth,
					textComponent.LineSpacing,
					textComponent.Kerning,
					textComponent.Color
				);
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
			Renderer::SubmitBillboards(translation, glm::vec2(1.0f), Renderer::GetInternalEditorIcon("PointLight"));
		}
	}

	void Scene::UpdateRectangularLightComponent(Timestep ts)
	{
		// Rectangular lights
		auto group = m_Registry.group<RectangularLightComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [rectangularLight, transformComponent] = group.get<RectangularLightComponent, TransformComponent>(entity);

			glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			Renderer::Submit(rectangularLight, translation, rotation, scale);
			Renderer::SubmitBillboards(translation, glm::vec2(1.0f), Renderer::GetInternalEditorIcon("RectangularLight"));
		}
	}

	void Scene::UpdateDirectionalLight(Timestep ts)
	{
		// Directional light
		auto group = m_Registry.group<DirectionalLightComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [directionalLight, transformComponent] = group.get<DirectionalLightComponent, TransformComponent>(entity);

			glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			Renderer::Submit(directionalLight, transformComponent.GetTransform()[2]);
			Renderer::SubmitBillboards(translation, glm::vec2(1.0f), Renderer::GetInternalEditorIcon("DirectionalLight"));
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

	void Scene::UpdateSceneCameras(Timestep ts)
	{
		// Scene Cameras
		auto group = m_Registry.group<CameraComponent>(entt::get<TransformComponent>);
		for (auto& entity : group)
		{
			auto [cameraComponent, transformComponent] = group.get<CameraComponent, TransformComponent>(entity);

			glm::mat4 transform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			Renderer::SubmitBillboards(translation, glm::vec2(1.0f), Renderer::GetInternalEditorIcon("SceneCamera"));

			if (m_SelectedEntity != entt::null)
			{
				if (m_SelectedEntity == entity)
				{
					cameraComponent.Camera->SetTransform(transform);

					const glm::mat4 inv = glm::inverse(cameraComponent.Camera->GetViewProjectionVK());
					glm::vec4 frustumCorners[8] =
					{
						// Near face
						{  1.0f,  1.0f, 0.0f, 1.0f },
						{ -1.0f,  1.0f, 0.0f, 1.0f },
						{  1.0f, -1.0f, 0.0f, 1.0f },
						{ -1.0f, -1.0f, 0.0f, 1.0f },

						// Far face
						{  1.0f,  1.0f, 1.0f, 1.0f },
						{ -1.0f,  1.0f, 1.0f, 1.0f },
						{  1.0f, -1.0f, 1.0f, 1.0f },
						{ -1.0f, -1.0f, 1.0f, 1.0f },
					};

					glm::vec3 v[8];
					for (uint32_t i = 0; i < 8; i++)
					{
						const glm::vec4 ff = inv * frustumCorners[i];
						v[i].x = ff.x / ff.w;
						v[i].y = ff.y / ff.w;
						v[i].z = ff.z / ff.w;
					}

					Renderer::SubmitLines(v[0], v[1], glm::vec4(1.0f));
					Renderer::SubmitLines(v[0], v[2], glm::vec4(1.0f));
					Renderer::SubmitLines(v[3], v[1], glm::vec4(1.0f));
					Renderer::SubmitLines(v[3], v[2], glm::vec4(1.0f));

					Renderer::SubmitLines(v[4], v[5], glm::vec4(1.0f));
					Renderer::SubmitLines(v[4], v[6], glm::vec4(1.0f));
					Renderer::SubmitLines(v[7], v[5], glm::vec4(1.0f));
					Renderer::SubmitLines(v[7], v[6], glm::vec4(1.0f));

					Renderer::SubmitLines(v[0], v[4], glm::vec4(1.0f));
					Renderer::SubmitLines(v[1], v[5], glm::vec4(1.0f));
					Renderer::SubmitLines(v[3], v[7], glm::vec4(1.0f));
					Renderer::SubmitLines(v[2], v[6], glm::vec4(1.0f));
				}
			}
		}
	}

	void Scene::UpdatePhysicsDebugMesh(Entity selectedEntity)
	{
		// Physics Debug Meshes
		if(selectedEntity.HasComponent<BoxColliderComponent>())
		{
			BoxColliderComponent& boxColliderComponent = selectedEntity.GetComponent<BoxColliderComponent>();

			glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(selectedEntity);
			globalTransform = glm::scale(glm::translate(globalTransform, boxColliderComponent.Offset * glm::vec3(2.0f)), boxColliderComponent.Size);

			Renderer::SubmitWireframeMesh(boxColliderComponent.DebugMesh, globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
		}


		if (selectedEntity.HasComponent<SphereColliderComponent>())
		{
			SphereColliderComponent& sphereColliderComponent = selectedEntity.GetComponent<SphereColliderComponent>();

			glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(selectedEntity);
			globalTransform = glm::scale(glm::translate(globalTransform, sphereColliderComponent.Offset * glm::vec3(1.0f)), glm::vec3(sphereColliderComponent.Radius * 2.0f));

			Renderer::SubmitWireframeMesh(sphereColliderComponent.DebugMesh, globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
		}


		if (selectedEntity.HasComponent<CapsuleColliderComponent>())
		{
			CapsuleColliderComponent& capsuleColliderComponent = selectedEntity.GetComponent<CapsuleColliderComponent>();

			glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(selectedEntity);

			Renderer::SubmitWireframeMesh(capsuleColliderComponent.DebugMesh, globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
		}

		if (selectedEntity.HasComponent<MeshColliderComponent>())
		{
			MeshColliderComponent& meshColliderComponent = selectedEntity.GetComponent<MeshColliderComponent>();

			if (meshColliderComponent.ProcessedMeshes.size())
			{
				glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(selectedEntity);

				for (uint32_t i = 0; i < meshColliderComponent.ProcessedMeshes.size(); i++)
				{
					if (meshColliderComponent.ProcessedMeshes[i])
						Renderer::SubmitWireframeMesh(meshColliderComponent.ProcessedMeshes[i], globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
				}

			}
		}

		if (selectedEntity.HasComponent<FogBoxVolumeComponent>())
		{
			FogBoxVolumeComponent& fogBoxVolumeComponent = selectedEntity.GetComponent<FogBoxVolumeComponent>();

			glm::mat4 transform = GetTransformMatFromEntityAndParent(selectedEntity);
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(transform, translation, rotation, scale);

			// Rescale by 2 the transform for the debug mesh to fit in the bounds of the fog box
			glm::mat4 rotationMat = glm::toMat4(glm::quat(glm::radians(rotation)));
			transform = glm::translate(glm::mat4(1.0f), translation) * rotationMat * glm::scale(glm::mat4(1.0f), 2.0f * scale);

			Renderer::SubmitWireframeMesh(MeshAsset::GetDefaultMeshes().Cube, transform, glm::vec4(0.3f, 0.6f, 0.9f, 1.0f), 2.0f);
		}
	}

	void Scene::UpdatePhysicsDebugMeshes(Timestep ts)
	{
		// Physics Debug Meshes
		auto groupBoxColliders = m_Registry.group<BoxColliderComponent>(entt::get<TransformComponent>);
		for (auto& entity : groupBoxColliders)
		{
			auto [boxColliderComponent, transformComponent] = groupBoxColliders.get<BoxColliderComponent, TransformComponent>(entity);

			glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			globalTransform = glm::scale(glm::translate(globalTransform, boxColliderComponent.Offset * glm::vec3(2.0f)), boxColliderComponent.Size);

			Renderer::SubmitWireframeMesh(boxColliderComponent.DebugMesh, globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
		}


		auto groupSphereColliders = m_Registry.group<SphereColliderComponent>(entt::get<TransformComponent>);
		for (auto& entity : groupSphereColliders)
		{
			auto [sphereColliderComponent, transformComponent] = groupSphereColliders.get<SphereColliderComponent, TransformComponent>(entity);

			glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(Entity(entity, this));
			globalTransform = glm::scale(glm::translate(globalTransform, sphereColliderComponent.Offset * glm::vec3(1.0f)), glm::vec3(sphereColliderComponent.Radius * 2.0f));

			Renderer::SubmitWireframeMesh(sphereColliderComponent.DebugMesh, globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
		}


		auto groupCapsuleColliders = m_Registry.group<CapsuleColliderComponent>(entt::get<TransformComponent>);
		for (auto& entity : groupCapsuleColliders)
		{
			auto [capsuleColliderComponent, transformComponent] = groupCapsuleColliders.get<CapsuleColliderComponent, TransformComponent>(entity);

			glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(Entity(entity, this));

			Renderer::SubmitWireframeMesh(capsuleColliderComponent.DebugMesh, globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);
		}

		auto groupMeshColliders = m_Registry.group<MeshColliderComponent>(entt::get<TransformComponent>);
		for (auto& entity : groupMeshColliders)
		{
			auto [meshColliderComponent, transformComponent] = groupMeshColliders.get<MeshColliderComponent, TransformComponent>(entity);

			if (meshColliderComponent.ProcessedMeshes.size())
			{
				glm::mat4 globalTransform = GetTransformMatFromEntityAndParent(Entity(entity, this));

				for (uint32_t i = 0; i < meshColliderComponent.ProcessedMeshes.size(); i++)
				{
					if(meshColliderComponent.ProcessedMeshes[i])
						Renderer::SubmitWireframeMesh(meshColliderComponent.ProcessedMeshes[i], globalTransform, glm::vec4(0.0f, 0.9f, 0.2f, 1.0f), 2.0f);

				}

			}
		}
	}

	void Scene::UpdateCSharpApplication(Timestep ts)
	{
		auto view = m_Registry.view<ScriptComponent>();
		for (auto entity : view)
		{
			Entity e = { entity, this };
			if (ScriptEngine::ModuleExists(e.GetComponent<ScriptComponent>().ModuleName))
			{
				ScriptEngine::OnUpdateEntity(e, ts);
			}
		}

		for (auto&& fn : m_PostUpdateQueue)
			fn();
		m_PostUpdateQueue.clear();
	}

	void Scene::OnScriptComponentConstruct(entt::registry& registry, entt::entity entity)
	{
		auto entityID = registry.get<IDComponent>(entity).ID;
		FROST_ASSERT_INTERNAL(bool(m_EntityIDMap.find(entityID) != m_EntityIDMap.end()));
		ScriptEngine::InitScriptEntity(m_EntityIDMap.at(entityID));
	}

	void Scene::OnScriptComponentDestroy(entt::registry& registry, entt::entity entity)
	{
		if (registry.any_of<IDComponent>(entity))
		{
			auto entityID = registry.get<IDComponent>(entity).ID;
			ScriptEngine::OnScriptComponentDestroyed(GetUUID(), entityID);
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

		m_EntityIDMap.erase(entity.GetComponent<IDComponent>().ID);
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

	void Scene::SubmitToDestroyEntity(Entity entity)
	{
		SubmitPostUpdateFunc([entity]() { entity.m_Scene->DestroyEntity(entity); });
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
		CopyComponent<ParentChildComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<TransformComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<MeshComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<AnimationComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CameraComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<PointLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<DirectionalLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SkyLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<RectangularLightComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<FogBoxVolumeComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CloudVolumeComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<RigidBodyComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<BoxColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<SphereColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<CapsuleColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<MeshColliderComponent>(target->m_Registry, m_Registry, enttMap);
		CopyComponent<ScriptComponent>(target->m_Registry, m_Registry, enttMap);

		//const auto& entityInstanceMap = ScriptEngine::GetEntityInstanceMap();
		//if (entityInstanceMap.find(this->GetUUID()) != entityInstanceMap.end())
		//	ScriptEngine::CopyEntityScriptData(target.Raw(), this);

#if 0
		target->m_Registry.each([&](auto entity)
		{
				Entity ent = { entity, target.Raw()};
				if (ent.HasComponent<ScriptComponent>())
				{
					ScriptComponent& scriptComponent = ent.GetComponent<ScriptComponent>();
					auto& moduleFieldMaps = scriptComponent.ModuleFieldMap;
					for (auto& [moduleName, moduleFieldMap] : moduleFieldMaps)
					{
						for (auto& [fieldName, fieldMap] : moduleFieldMap)
						{
							if (fieldMap.Type == FieldType::String)
							{
								std::string storedValueStr = fieldMap.GetStoredValue<std::string>();
								fieldMap.SetStoredValue<std::string>(storedValueStr);
							}
						}
					}
				}
		});
#endif

		// Sort IdComponent by by entity handle (which is essentially the order in which they were created)
		// This ensures a consistent ordering when iterating IdComponent (for example: when rendering scene hierarchy panel)
		//target->m_Registry.sort<IDComponent>([&target](const auto lhs, const auto rhs)
		//{
		//	auto lhsEntity = target->GetEntityMap().find(lhs.ID);
		//	auto rhsEntity = target->GetEntityMap().find(rhs.ID);
		//	return static_cast<uint32_t>(lhsEntity->second) < static_cast<uint32_t>(rhsEntity->second);
		//});
	}

#if 1
	void Scene::SetSelectedEntity(Entity entity)
	{
		//m_SelectedEntity = entity.Raw();
		//Renderer::SetEditorActiveEntity((uint32_t)m_SelectedEntity);
		m_SelectedEntity = entity;

		//if (entity)
		//{
		//	if (m_EntityIDMap.find(entity.GetUUID()) != m_EntityIDMap.end())
		//	{
		//	}
		//}
	}
#endif

	void Scene::ClearScene()
	{
		m_EntityIDMap.clear();
		m_SelectedEntity = {};
		m_Registry.clear();
	}

	Ref<Scene> Scene::CreateEmpty()
	{
		return Ref<Scene>::Create("Empty", false);
	}

}