#include "frostpch.h"
#include "SceneSerializer.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Renderer/Mesh.h"

#include "Frost/Renderer/BindlessAllocator.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Physics/PhysX/CookingFactory.h"
#include "Frost/Script/ScriptEngine.h"

#include "Frost/EntitySystem/Entity.h"

namespace Frost
{
	static std::string GetNameFromFilepath(const std::string& filepath);
	static std::string GetNameFromFieldType(FieldType type);
	static FieldType GetFieldTypeFromName(const std::string& fieldTypeStr);

	bool SceneSerializer::TryLoadData(const AssetMetadata& metadata, Ref<Asset>& asset, void* pNext) const
	{
		Ref<Scene> scene = Ref<Scene>::Create(); // Maybe not create a new scene and leave the one passed in parameters?
		DeserializeScene(AssetManager::GetFileSystemPathString(metadata), scene);

		asset = scene;
		asset->Handle = metadata.Handle;

		return true;
	}

	void SceneSerializer::SerializeScene(const std::string& filepath, Ref<Scene> scene)
	{
		nlohmann::ordered_json out = nlohmann::ordered_json();

		// This loop is in reversed order (because this is how the entt libraries handles each loop)
		// so we should firstly store the entities in the reverse order in an vector, and then read that vector in reverse
		Vector<Entity> entities;
		scene->m_Registry.each([&](auto entity)
		{
			Entity ent = { entity, scene.Raw() };
			if (ent)
				entities.push_back(ent);
		});
		for (int32_t i = entities.size() - 1; i >= 0; i--)
		{
			SerializeEntity(out, entities[i]);
		}

		std::ofstream istream(filepath);
		istream.clear();
		istream << out.dump(4);
 		istream.close();
	}

	void SceneSerializer::SerializeEntity(nlohmann::ordered_json& out, Entity entity)
	{
		uint64_t uuid = entity.GetComponent<IDComponent>().ID.Get();
		nlohmann::ordered_json entityOut = nlohmann::ordered_json();

		entityOut["UUID"] = uuid;

		if (entity.HasComponent<TagComponent>())
		{
			entityOut["TagComponent"] = entity.GetComponent<TagComponent>().Tag;
		}

		if (entity.HasComponent<ParentChildComponent>())
		{
			ParentChildComponent& parentChildComponent = entity.GetComponent<ParentChildComponent>();

			entityOut["ParentChildComponent"]["ParentID"] = parentChildComponent.ParentID.Get();

			entityOut["ParentChildComponent"]["ChildIDs"] = nlohmann::json();
			for (auto& uuid : parentChildComponent.ChildIDs)
			{
				entityOut["ParentChildComponent"]["ChildIDs"].push_back(uuid.Get());
			}
		}

		if (entity.HasComponent<TransformComponent>())
		{
			TransformComponent& transform = entity.GetComponent<TransformComponent>();

			nlohmann::json translation = {transform.Translation.x, transform.Translation.y, transform.Translation.z};
			nlohmann::json rotation = { transform.Rotation.x, transform.Rotation.y, transform.Rotation.z };
			nlohmann::json scale = { transform.Scale.x, transform.Scale.y, transform.Scale.z };

			entityOut["TransformComponent"]["Translation"] = translation;
			entityOut["TransformComponent"]["Rotation"] = rotation;
			entityOut["TransformComponent"]["Scale"] = scale;
		}

		if (entity.HasComponent<MeshComponent>())
		{
			MeshComponent& meshComponent = entity.GetComponent<MeshComponent>();
			Ref<Mesh> mesh = meshComponent.Mesh;

			nlohmann::json materials = nlohmann::json();
			std::string meshFilepath = "";


			if (mesh)
			{
				meshFilepath = AssetManager::GetRelativePath(mesh->GetMeshAsset()->GetFilepath()).string();
				std::replace(meshFilepath.begin(), meshFilepath.end(), '\\', '/');

				for (uint32_t k = 0; k < mesh->GetMaterialCount(); k++)
				{
					// Setting up the material data into a storage buffer
					Ref<MaterialAsset> materialAsset = mesh->GetMaterialAsset(k);

					//glm::vec4 albedoColor = materialData.Get<glm::vec4>("AlbedoColor");
					nlohmann::json material = nlohmann::json();

					material["MaterialIndex"] = k;
					material["AssetID"] = materialAsset->Handle.Get(); // Default

					materials.push_back(material);
				}

			}

			entityOut["MeshComponent"]["Materials"] = materials;
			entityOut["MeshComponent"]["Filepath"] = meshFilepath;
		}

		if (entity.HasComponent<AnimationComponent>())
		{
			int32_t animationIndex = -1;

			AnimationComponent& animationController = entity.GetComponent<AnimationComponent>();

			// Check whether the entity has a mesh component or not. If it does then search for the animation index
			if (entity.HasComponent<MeshComponent>())
			{
				MeshComponent& meshComponent = entity.GetComponent<MeshComponent>();
				Ref<MeshAsset> mesh = meshComponent.Mesh->GetMeshAsset();

				if (mesh)
				{
					for (uint32_t i = 0; i < mesh->m_Animations.size(); i++)
					{
						if (mesh->m_Animations[i].Raw() == animationController.Controller->GetActiveAnimation().Raw())
						{
							animationIndex = i;
							break;
						}
					}
				}
			}
			entityOut["AnimationComponent"]["AnimationIndex"] = animationIndex;
		}

		if (entity.HasComponent<SkyLightComponent>())
		{
			SkyLightComponent& skyLightComponent = entity.GetComponent<SkyLightComponent>();

			entityOut["SkyLightComponent"]["Filepath"] = skyLightComponent.Filepath;
			entityOut["SkyLightComponent"]["IsActive"] = skyLightComponent.IsActive;
		}

		if (entity.HasComponent<RigidBodyComponent>())
		{
			RigidBodyComponent& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

			entityOut["RigidBodyComponent"]["BodyType"] = ((rigidBodyComponent.BodyType == RigidBodyComponent::Type::Static) ? "Static" : "Dynamic");
			entityOut["RigidBodyComponent"]["Mass"] = rigidBodyComponent.Mass;
			entityOut["RigidBodyComponent"]["LinearDrag"] = rigidBodyComponent.LinearDrag;
			entityOut["RigidBodyComponent"]["AngularDrag"] = rigidBodyComponent.AngularDrag;
			entityOut["RigidBodyComponent"]["DisableGravity"] = rigidBodyComponent.DisableGravity;
			entityOut["RigidBodyComponent"]["IsKinematic"] = rigidBodyComponent.IsKinematic;
			entityOut["RigidBodyComponent"]["Layer"] = rigidBodyComponent.Layer;
			entityOut["RigidBodyComponent"]["LockPositionX"] = rigidBodyComponent.LockPositionX;
			entityOut["RigidBodyComponent"]["LockPositionY"] = rigidBodyComponent.LockPositionY;
			entityOut["RigidBodyComponent"]["LockPositionZ"] = rigidBodyComponent.LockPositionZ;
			entityOut["RigidBodyComponent"]["LockRotationX"] = rigidBodyComponent.LockRotationX;
			entityOut["RigidBodyComponent"]["LockRotationY"] = rigidBodyComponent.LockRotationY;
			entityOut["RigidBodyComponent"]["LockRotationZ"] = rigidBodyComponent.LockRotationZ;
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			BoxColliderComponent& boxColliderComponent = entity.GetComponent<BoxColliderComponent>();

			entityOut["BoxColliderComponent"]["Size"] = { boxColliderComponent.Size.x, boxColliderComponent.Size.y, boxColliderComponent.Size.z };
			entityOut["BoxColliderComponent"]["Offset"] = { boxColliderComponent.Offset.x, boxColliderComponent.Offset.y, boxColliderComponent.Offset.z };
			entityOut["BoxColliderComponent"]["IsTrigger"] = boxColliderComponent.IsTrigger;
			entityOut["BoxColliderComponent"]["MaterialAssetID"] = boxColliderComponent.MaterialHandle ? boxColliderComponent.MaterialHandle->Handle.Get() : 0;
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			SphereColliderComponent& sphereColliderComponent = entity.GetComponent<SphereColliderComponent>();

			entityOut["SphereColliderComponent"]["Radius"] = sphereColliderComponent.Radius;
			entityOut["SphereColliderComponent"]["Offset"] = { sphereColliderComponent.Offset.x, sphereColliderComponent.Offset.y, sphereColliderComponent.Offset.z };
			entityOut["SphereColliderComponent"]["IsTrigger"] = sphereColliderComponent.IsTrigger;
			entityOut["SphereColliderComponent"]["MaterialAssetID"] = sphereColliderComponent.MaterialHandle ? sphereColliderComponent.MaterialHandle->Handle.Get() : 0;
		}

		if (entity.HasComponent<CapsuleColliderComponent>())
		{
			CapsuleColliderComponent& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();

			entityOut["CapsuleColliderComponent"]["Radius"] = capsuleColliderComponent.Radius;
			entityOut["CapsuleColliderComponent"]["Height"] = capsuleColliderComponent.Height;
			entityOut["CapsuleColliderComponent"]["Offset"] = { capsuleColliderComponent.Offset.x, capsuleColliderComponent.Offset.y, capsuleColliderComponent.Offset.z };
			entityOut["CapsuleColliderComponent"]["IsTrigger"] = capsuleColliderComponent.IsTrigger;
			entityOut["CapsuleColliderComponent"]["MaterialAssetID"] = capsuleColliderComponent.MaterialHandle ? capsuleColliderComponent.MaterialHandle->Handle.Get() : 0;
		}

		if (entity.HasComponent<MeshColliderComponent>())
		{
			MeshColliderComponent& meshColliderComponent = entity.GetComponent<MeshColliderComponent>();

			entityOut["MeshColliderComponent"]["IsConvex"] = meshColliderComponent.IsConvex;
			entityOut["MeshColliderComponent"]["IsTrigger"] = meshColliderComponent.IsTrigger;
			entityOut["MeshColliderComponent"]["MaterialAssetID"] = meshColliderComponent.MaterialHandle ? meshColliderComponent.MaterialHandle->Handle.Get() : 0;
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			DirectionalLightComponent& dirLightComponent = entity.GetComponent<DirectionalLightComponent>();

			entityOut["DirectionalLightComponent"]["Color"] = { dirLightComponent.Color.x, dirLightComponent.Color.y, dirLightComponent.Color.z };
			entityOut["DirectionalLightComponent"]["Intensity"] = dirLightComponent.Intensity;
			entityOut["DirectionalLightComponent"]["Size"] = dirLightComponent.Size;

			entityOut["DirectionalLightComponent"]["VolumeDensity"] = dirLightComponent.VolumeDensity;
			entityOut["DirectionalLightComponent"]["Absorption"] = dirLightComponent.Absorption;
			entityOut["DirectionalLightComponent"]["Phase"] = dirLightComponent.Phase;
		}

		if (entity.HasComponent<PointLightComponent>())
		{
			PointLightComponent& pointLightComponent = entity.GetComponent<PointLightComponent>();

			entityOut["PointLightComponent"]["Color"] = { pointLightComponent.Color.x, pointLightComponent.Color.y, pointLightComponent.Color.z };
			entityOut["PointLightComponent"]["Intensity"] = pointLightComponent.Intensity;
			entityOut["PointLightComponent"]["Radius"] = pointLightComponent.Radius;
			entityOut["PointLightComponent"]["Falloff"] = pointLightComponent.Falloff;
		}

		if (entity.HasComponent<RectangularLightComponent>())
		{
			RectangularLightComponent& rectLightComponent = entity.GetComponent<RectangularLightComponent>();

			entityOut["RectangularLightComponent"]["Radiance"] = { rectLightComponent.Radiance.x, rectLightComponent.Radiance.y, rectLightComponent.Radiance.z };
			entityOut["RectangularLightComponent"]["Intensity"] = rectLightComponent.Intensity;
			entityOut["RectangularLightComponent"]["Radius"] = rectLightComponent.Radius;
			entityOut["RectangularLightComponent"]["TwoSided"] = rectLightComponent.TwoSided;
		}

		if (entity.HasComponent<FogBoxVolumeComponent>())
		{
			FogBoxVolumeComponent& fogBoxVolumeComponent = entity.GetComponent<FogBoxVolumeComponent>();

			entityOut["FogBoxVolumeComponent"]["MieScattering"] = { fogBoxVolumeComponent.MieScattering.x, fogBoxVolumeComponent.MieScattering.y, fogBoxVolumeComponent.MieScattering.z };
			entityOut["FogBoxVolumeComponent"]["PhaseValue"] = fogBoxVolumeComponent.PhaseValue;
			entityOut["FogBoxVolumeComponent"]["Emission"] = { fogBoxVolumeComponent.Emission.x, fogBoxVolumeComponent.Emission.y, fogBoxVolumeComponent.Emission.z };
			entityOut["FogBoxVolumeComponent"]["Absorption"] = fogBoxVolumeComponent.Absorption;
			entityOut["FogBoxVolumeComponent"]["Density"] = fogBoxVolumeComponent.Density;
		}

		if (entity.HasComponent<CloudVolumeComponent>())
		{
			CloudVolumeComponent& cloudVolumeComponent = entity.GetComponent<CloudVolumeComponent>();

			entityOut["CloudVolumeComponent"]["CloudScale"] = cloudVolumeComponent.CloudScale;
			entityOut["CloudVolumeComponent"]["Density"] = cloudVolumeComponent.Density;

			entityOut["CloudVolumeComponent"]["Scattering"] = { cloudVolumeComponent.Scattering.x, cloudVolumeComponent.Scattering.y, cloudVolumeComponent.Scattering.z };
			entityOut["CloudVolumeComponent"]["PhaseFunction"] = cloudVolumeComponent.PhaseFunction;
			
			entityOut["CloudVolumeComponent"]["DensityOffset"] = cloudVolumeComponent.DensityOffset;
			entityOut["CloudVolumeComponent"]["DetailOffset"] = cloudVolumeComponent.DetailOffset;
			entityOut["CloudVolumeComponent"]["CloudAbsorption"] = cloudVolumeComponent.CloudAbsorption;
			entityOut["CloudVolumeComponent"]["SunAbsorption"] = cloudVolumeComponent.SunAbsorption;
		}

		if (entity.HasComponent<CameraComponent>())
		{
			CameraComponent& cameraComponent = entity.GetComponent<CameraComponent>();

			entityOut["CameraComponent"]["FOV"] = cameraComponent.Camera->GetCameraFOV();
			entityOut["CameraComponent"]["NearClip"] = cameraComponent.Camera->GetNearClip();
			entityOut["CameraComponent"]["FarClip"] = cameraComponent.Camera->GetFarClip();
			entityOut["CameraComponent"]["Primary"] = cameraComponent.Primary;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			ScriptComponent& scriptComponent = entity.GetComponent<ScriptComponent>();

			entityOut["ScriptComponent"]["ModuleName"] = scriptComponent.ModuleName;

			const HashMap<std::string, PublicField>& fieldMap = scriptComponent.ModuleFieldMap[scriptComponent.ModuleName];
			for (auto& [fieldName, field] : fieldMap)
			{
				FieldType fieldType = field.Type;

				nlohmann::ordered_json jsonPublicField = nlohmann::ordered_json();

				jsonPublicField["FieldName"] = fieldName;
				jsonPublicField["Type"] = GetNameFromFieldType(fieldType);

				switch (fieldType)
				{
					case FieldType::Float:           jsonPublicField["Value"] = field.GetStoredValue<float>(); break;
					case FieldType::Int:             jsonPublicField["Value"] = field.GetStoredValue<int32_t>(); break;
					case FieldType::UnsignedInt:     jsonPublicField["Value"] = field.GetStoredValue<uint32_t>(); break;
					case FieldType::String:          jsonPublicField["Value"] = field.GetStoredValue<const std::string&>(); break;
					case FieldType::Vec2:
					{
						glm::vec2 fieldVec2 = field.GetStoredValue<glm::vec2>();
						nlohmann::json jsonFieldVec2 = { fieldVec2.x, fieldVec2.y };
						jsonPublicField["Value"] = jsonFieldVec2;
						break;
					}
					case FieldType::Vec3:
					{
						glm::vec3 fieldVec3 = field.GetStoredValue<glm::vec3>();
						nlohmann::json jsonFieldVec3 = { fieldVec3.x, fieldVec3.y, fieldVec3.z };
						jsonPublicField["Value"] = jsonFieldVec3;
						break;
					}
					case FieldType::Vec4:
					{
						glm::vec4 fieldVec4 = field.GetStoredValue<glm::vec4>();
						nlohmann::json jsonFieldVec4 = { fieldVec4.x, fieldVec4.y, fieldVec4.z, fieldVec4.z };
						jsonPublicField["Value"] = jsonFieldVec4;
						break;
					}
					case FieldType::ClassReference: break;// TODO: Not sure if storing a class reference would even make sense, because eventually the reference will differ
					case FieldType::Asset: break; // TODO: Add assets (prefabs in our case)

					case FieldType::Entity:
					{
						UUID entityUUID = field.GetStoredValue<UUID>();
						jsonPublicField["Value"] = entityUUID.Get();
						break;
					}

					case FieldType::None:
					default: FROST_ASSERT_MSG("Field Type is not valid!");
				}

				entityOut["ScriptComponent"]["PublicFields"].push_back(jsonPublicField);
			}

		}

		

		out.push_back(entityOut);
	}

	void SceneSerializer::DeserializeScene(const std::string& filepath, Ref<Scene>& scene)
	{
		std::string content;
		std::ifstream instream(filepath);
		
		// Get the scene name from filepath
		instream.seekg(0, std::ios::end);
		size_t size = instream.tellg();
		content.resize(size);

		instream.seekg(0, std::ios::beg);
		instream.read(&content[0], size);
		instream.close();

		// Parse the json file
		nlohmann::json in = nlohmann::json::parse(content);

		// Loop through every entity and add its components
		for (auto& entity : in)
		{
			uint64_t uuid = entity["UUID"];

			Entity ent = scene->CreateEntityWithID(uuid);
			ent.RemoveComponent<TransformComponent>();

			// Tag Component
			TagComponent& tagComponent = ent.GetComponent<TagComponent>();
			tagComponent.Tag = entity["TagComponent"];

			// Parent Child Component
			ParentChildComponent& parentChildComponent = ent.GetComponent<ParentChildComponent>();
			parentChildComponent.ParentID = UUID(entity["ParentChildComponent"]["ParentID"]);

			for (auto& child : entity["ParentChildComponent"]["ChildIDs"])
			{
				parentChildComponent.ChildIDs.push_back(UUID(child));
			}

			// Transform Component
			if (!entity["TransformComponent"].is_null())
			{
				TransformComponent& transformComponent = ent.AddComponent<TransformComponent>();

				std::vector<float> translation = entity["TransformComponent"]["Translation"];
				std::vector<float> rotation = entity["TransformComponent"]["Rotation"];
				std::vector<float> scale = entity["TransformComponent"]["Scale"];
					
				transformComponent.Translation = { translation[0], translation[1], translation[2] };
				transformComponent.Rotation = { rotation[0], rotation[1], rotation[2] };
				transformComponent.Scale = { scale[0], scale[1], scale[2] };
			}

			// Transform Component
			if (!entity["MeshComponent"].is_null())
			{
				MeshComponent& meshComponent = ent.AddComponent<MeshComponent>();

				Ref<MeshAsset> meshAsset = AssetManager::GetOrLoadAsset<MeshAsset>(entity["MeshComponent"]["Filepath"]);
				if (meshAsset)
				{
					meshComponent.Mesh = Ref<Mesh>::Create(meshAsset);

					uint32_t materialCount = entity["MeshComponent"]["Materials"].size();
					for (uint32_t materialIndex = 0; materialIndex < materialCount; materialIndex++)
					{
						nlohmann::json materialsIn = entity["MeshComponent"]["Materials"][materialIndex];

						UUID materialAssetId = UUID(materialsIn["AssetID"]);
						if (materialAssetId == 0)
							continue;

						const AssetMetadata& materialAssetMetadata = AssetManager::GetMetadata(materialAssetId);
						Ref<MaterialAsset> materialAsset = AssetManager::GetOrLoadAsset<MaterialAsset>(materialAssetMetadata.FilePath.string());

						if (materialAsset)
						{
							meshComponent.Mesh->SetMaterialByAsset(materialIndex, materialAsset);
						}
						else
						{
							FROST_CORE_ERROR("Material with UUID {0} not found in the Asset Registry!", materialAssetId.Get());
						}
					}
				}
				else
				{
					FROST_CORE_ERROR("Mesh Asset with filepath '{0}' and UUID {1} not found in the Asset Registry!", entity["MeshComponent"]["Filepath"], meshAsset->Handle);
				}
			}

			// Animation Component
			if (!entity["AnimationComponent"].is_null())
			{
				AnimationComponent& animationComponent = ent.AddComponent<AnimationComponent>(nullptr);
				nlohmann::json cloudVolumeIn = entity["AnimationComponent"];

				if (ent.HasComponent<MeshComponent>())
				{
					MeshComponent& meshComponent = ent.GetComponent<MeshComponent>();
					Ref<MeshAsset> mesh = meshComponent.Mesh->GetMeshAsset();

					if (mesh)
					{
						animationComponent.MeshComponentPtr = &meshComponent;
						animationComponent.Controller = nullptr;

						if (entity["AnimationComponent"]["AnimationIndex"] != -1)
						{
							animationComponent.Controller = CreateRef<AnimationController>();
							animationComponent.Controller->SetActiveAnimation(mesh->GetAnimations()[entity["AnimationComponent"]["AnimationIndex"]]);
						}
					}
					else
					{
						animationComponent.MeshComponentPtr = nullptr;
						animationComponent.Controller = nullptr;
					}
				}
				else
				{
					animationComponent.MeshComponentPtr = nullptr;
					animationComponent.Controller = nullptr;
				}

			}

			// Sky Light Component
			if (!entity["SkyLightComponent"].is_null())
			{
				SkyLightComponent& skyLightComponent = ent.AddComponent<SkyLightComponent>();
				skyLightComponent.Filepath = entity["SkyLightComponent"]["Filepath"];
				skyLightComponent.IsActive = entity["SkyLightComponent"]["IsActive"];

				if (!skyLightComponent.Filepath.empty())
				{
					bool computeEnvMap = Renderer::GetSceneEnvironment()->ComputeEnvironmentMap(
						skyLightComponent.Filepath, skyLightComponent.RadianceMap, skyLightComponent.PrefilteredMap, skyLightComponent.IrradianceMap
					);
					if (!computeEnvMap)
						skyLightComponent.IsActive = false;
				}
			}

			// Rigid Body Component
			if (!entity["RigidBodyComponent"].is_null())
			{
				RigidBodyComponent& rigidBodyComponent = ent.AddComponent<RigidBodyComponent>();
				nlohmann::json rigidBodyIn = entity["RigidBodyComponent"];

				rigidBodyComponent.BodyType = rigidBodyIn["BodyType"] == "Static" ? RigidBodyComponent::Type::Static : RigidBodyComponent::Type::Dynamic;
				rigidBodyComponent.Mass = rigidBodyIn["Mass"];
				rigidBodyComponent.LinearDrag = rigidBodyIn["LinearDrag"];
				rigidBodyComponent.AngularDrag = rigidBodyIn["AngularDrag"];
				rigidBodyComponent.DisableGravity = rigidBodyIn["DisableGravity"];
				rigidBodyComponent.IsKinematic = rigidBodyIn["IsKinematic"];
				rigidBodyComponent.Layer = rigidBodyIn["Layer"];
				rigidBodyComponent.LockPositionX = rigidBodyIn["LockPositionX"];
				rigidBodyComponent.LockPositionY = rigidBodyIn["LockPositionY"];
				rigidBodyComponent.LockPositionZ = rigidBodyIn["LockPositionZ"];
				rigidBodyComponent.LockRotationX = rigidBodyIn["LockRotationX"];
				rigidBodyComponent.LockRotationY = rigidBodyIn["LockRotationY"];
				rigidBodyComponent.LockRotationZ = rigidBodyIn["LockRotationZ"];
			}

			// Box Collider Component
			if (!entity["BoxColliderComponent"].is_null())
			{
				BoxColliderComponent& boxColliderComponent = ent.AddComponent<BoxColliderComponent>();
				nlohmann::json boxColliderIn = entity["BoxColliderComponent"];

				boxColliderComponent.MaterialHandle = nullptr; // We will check later if the component has a physics material
				boxColliderComponent.Size = { boxColliderIn["Size"][0], boxColliderIn["Size"][1], boxColliderIn["Size"][2] };
				boxColliderComponent.Offset = { boxColliderIn["Offset"][0], boxColliderIn["Offset"][1], boxColliderIn["Offset"][2] };
				boxColliderComponent.IsTrigger = boxColliderIn["IsTrigger"];

				UUID materialAssetId = UUID(boxColliderIn["MaterialAssetID"]);
				if (materialAssetId != 0)
				{
					const AssetMetadata& materialAssetMetadata = AssetManager::GetMetadata(materialAssetId);
					Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialAssetMetadata.FilePath.string());

					if (physicsMaterialAsset)
					{
						boxColliderComponent.MaterialHandle = physicsMaterialAsset;
					}
					else
					{
						FROST_CORE_ERROR("Physics Material with UUID {0} not found in the Asset Registry!", materialAssetId.Get());
					}
				}

				
			}

			// Sphere Collider Component
			if (!entity["SphereColliderComponent"].is_null())
			{
				SphereColliderComponent& sphereColliderComponent = ent.AddComponent<SphereColliderComponent>();
				nlohmann::json sphereColliderIn = entity["SphereColliderComponent"];

				sphereColliderComponent.MaterialHandle = nullptr; // We will check later if the component has a physics material
				sphereColliderComponent.Radius = sphereColliderIn["Radius"];
				sphereColliderComponent.Offset = { sphereColliderIn["Offset"][0], sphereColliderIn["Offset"][1], sphereColliderIn["Offset"][2] };
				sphereColliderComponent.IsTrigger = sphereColliderIn["IsTrigger"];

				UUID materialAssetId = UUID(sphereColliderIn["MaterialAssetID"]);
				if (materialAssetId != 0)
				{
					const AssetMetadata& materialAssetMetadata = AssetManager::GetMetadata(materialAssetId);
					Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialAssetMetadata.FilePath.string());

					if (physicsMaterialAsset)
					{
						sphereColliderComponent.MaterialHandle = physicsMaterialAsset;
					}
					else
					{
						FROST_CORE_ERROR("Physics Material with UUID {0} not found in the Asset Registry!", materialAssetId.Get());
					}
				}
			}

			// Capsule Collider Component
			if (!entity["CapsuleColliderComponent"].is_null())
			{
				CapsuleColliderComponent& capsuleColliderComponent = ent.AddComponent<CapsuleColliderComponent>();
				nlohmann::json capsuleColliderIn = entity["CapsuleColliderComponent"];

				capsuleColliderComponent.MaterialHandle = nullptr; // We will check later if the component has a physics material
				capsuleColliderComponent.Radius = capsuleColliderIn["Radius"];
				capsuleColliderComponent.Height = capsuleColliderIn["Height"];
				capsuleColliderComponent.Offset = { capsuleColliderIn["Offset"][0], capsuleColliderIn["Offset"][1], capsuleColliderIn["Offset"][2] };
				capsuleColliderComponent.IsTrigger = capsuleColliderIn["IsTrigger"];

				UUID materialAssetId = UUID(capsuleColliderIn["MaterialAssetID"]);
				if (materialAssetId != 0)
				{
					const AssetMetadata& materialAssetMetadata = AssetManager::GetMetadata(materialAssetId);
					Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialAssetMetadata.FilePath.string());

					if (physicsMaterialAsset)
					{
						capsuleColliderComponent.MaterialHandle = physicsMaterialAsset;
					}
					else
					{
						FROST_CORE_ERROR("Physics Material with UUID {0} not found in the Asset Registry!", materialAssetId.Get());
					}
				}
			}

			// Mesh Collider Component
			if (!entity["MeshColliderComponent"].is_null())
			{
				MeshColliderComponent& meshColliderComponent = ent.AddComponent<MeshColliderComponent>();
				nlohmann::json meshColliderIn = entity["MeshColliderComponent"];

				meshColliderComponent.MaterialHandle = nullptr; // We will check later if the component has a physics material
				meshColliderComponent.IsConvex = meshColliderIn["IsConvex"];
				meshColliderComponent.IsTrigger = meshColliderIn["IsTrigger"];

				// Add the debug meshes
				if (ent.HasComponent<MeshComponent>())
				{
					MeshComponent& meshComponent = ent.GetComponent<MeshComponent>();
					if (meshComponent.IsMeshValid())
					{
						if (!meshComponent.Mesh->IsAnimated())
						{
							meshColliderComponent.CollisionMesh = meshComponent.Mesh->GetMeshAsset();
							CookingResult result = CookingFactory::CookMesh(meshColliderComponent, false);
						}
						else
						{
							meshColliderComponent.ResetMesh();
							FROST_CORE_WARN("[SceneSerializer] Mesh Colliders don't support dynamic meshes!");
						}
					}
				}

				UUID materialAssetId = UUID(meshColliderIn["MaterialAssetID"]);
				if (materialAssetId != 0)
				{
					const AssetMetadata& materialAssetMetadata = AssetManager::GetMetadata(materialAssetId);
					Ref<PhysicsMaterial> physicsMaterialAsset = AssetManager::GetOrLoadAsset<PhysicsMaterial>(materialAssetMetadata.FilePath.string());

					if (physicsMaterialAsset)
					{
						meshColliderComponent.MaterialHandle = physicsMaterialAsset;
					}
					else
					{
						FROST_CORE_ERROR("Physics Material with UUID {0} not found in the Asset Registry!", materialAssetId.Get());
					}
				}
			}

			// Directional Light Component
			if (!entity["DirectionalLightComponent"].is_null())
			{
				DirectionalLightComponent& dirLightComponent = ent.AddComponent<DirectionalLightComponent>();
				nlohmann::json dirLightIn = entity["DirectionalLightComponent"];

				dirLightComponent.Color = { dirLightIn["Color"][0], dirLightIn["Color"][1], dirLightIn["Color"][2] };
				dirLightComponent.Intensity = dirLightIn["Intensity"];
				dirLightComponent.Size = dirLightIn["Size"];

				dirLightComponent.VolumeDensity = dirLightIn["VolumeDensity"];
				dirLightComponent.Absorption = dirLightIn["Absorption"];
				dirLightComponent.Phase = dirLightIn["Phase"];
			}

			// Point Light Component
			if (!entity["PointLightComponent"].is_null())
			{
				PointLightComponent& pointLightComponent = ent.AddComponent<PointLightComponent>();
				nlohmann::json pointLightIn = entity["PointLightComponent"];

				pointLightComponent.Color = { pointLightIn["Color"][0], pointLightIn["Color"][1], pointLightIn["Color"][2] };
				pointLightComponent.Intensity = pointLightIn["Intensity"];
				pointLightComponent.Radius = pointLightIn["Radius"];
				pointLightComponent.Falloff = pointLightIn["Falloff"];
			}

			// Rectangular Light Component
			if (!entity["RectangularLightComponent"].is_null())
			{
				RectangularLightComponent& rectLightComponent = ent.AddComponent<RectangularLightComponent>();
				nlohmann::json rectLightIn = entity["RectangularLightComponent"];

				rectLightComponent.Radiance = { rectLightIn["Radiance"][0], rectLightIn["Radiance"][1], rectLightIn["Radiance"][2] };
				rectLightComponent.Intensity = rectLightIn["Intensity"];
				rectLightComponent.Radius = rectLightIn["Radius"];
				rectLightComponent.TwoSided = rectLightIn["TwoSided"];
			}

			// Fog Box Volume Component
			if (!entity["FogBoxVolumeComponent"].is_null())
			{
				FogBoxVolumeComponent& fogBoxVolumeComponent = ent.AddComponent<FogBoxVolumeComponent>();
				nlohmann::json fogBoxVolumeIn = entity["FogBoxVolumeComponent"];

				fogBoxVolumeComponent.MieScattering = { fogBoxVolumeIn["MieScattering"][0], fogBoxVolumeIn["MieScattering"][1], fogBoxVolumeIn["MieScattering"][2] };
				fogBoxVolumeComponent.PhaseValue = fogBoxVolumeIn["PhaseValue"];
				fogBoxVolumeComponent.Emission = { fogBoxVolumeIn["Emission"][0], fogBoxVolumeIn["Emission"][1], fogBoxVolumeIn["Emission"][2] };
				fogBoxVolumeComponent.Absorption = fogBoxVolumeIn["Absorption"];
				fogBoxVolumeComponent.Density = fogBoxVolumeIn["Density"];
			}

			// Cloud Volume Component
			if (!entity["CloudVolumeComponent"].is_null())
			{
				CloudVolumeComponent& cloudVolumeComponent = ent.AddComponent<CloudVolumeComponent>();
				nlohmann::json cloudVolumeIn = entity["CloudVolumeComponent"];

				cloudVolumeComponent.CloudScale = cloudVolumeIn["CloudScale"];
				cloudVolumeComponent.Density = cloudVolumeIn["Density"];
				cloudVolumeComponent.Scattering = { cloudVolumeIn["Scattering"][0], cloudVolumeIn["Scattering"][1], cloudVolumeIn["Scattering"][2] };
				cloudVolumeComponent.PhaseFunction = cloudVolumeIn["PhaseFunction"];
				cloudVolumeComponent.DensityOffset = cloudVolumeIn["DensityOffset"];
				cloudVolumeComponent.DetailOffset = cloudVolumeIn["DetailOffset"];
				cloudVolumeComponent.CloudAbsorption = cloudVolumeIn["CloudAbsorption"];
				cloudVolumeComponent.SunAbsorption = cloudVolumeIn["SunAbsorption"];
			}

			if (!entity["CameraComponent"].is_null())
			{
				CameraComponent& cameraComponent = ent.AddComponent<CameraComponent>();

				cameraComponent.Camera->SetFOV(entity["CameraComponent"]["FOV"]);
				cameraComponent.Camera->SetNearClip(entity["CameraComponent"]["NearClip"]);
				cameraComponent.Camera->SetFarClip(entity["CameraComponent"]["FarClip"]);
				cameraComponent.Camera->RecalculateProjectionMatrix();

				cameraComponent.Primary = entity["CameraComponent"]["Primary"];
			}

			if (!entity["ScriptComponent"].is_null())
			{
				ScriptComponent& scriptComponent = ent.AddComponent<ScriptComponent>();

				scriptComponent.ModuleName = entity["ScriptComponent"]["ModuleName"];
				auto& fieldMap = scriptComponent.ModuleFieldMap[scriptComponent.ModuleName];
				
				ScriptEngine::InitScriptEntity(ent);
				ScriptEngine::InstantiateEntityClass(ent);

				uint32_t publicFieldsSize = entity["ScriptComponent"]["PublicFields"].size();
				for (uint32_t i = 0; i < publicFieldsSize; i++)
				{
					nlohmann::json publicFieldJson = entity["ScriptComponent"]["PublicFields"][i];

					if (fieldMap.find(publicFieldJson["FieldName"]) != fieldMap.end())
					{
						PublicField& publicField = fieldMap[publicFieldJson["FieldName"]];

						FieldType publicFieldType = GetFieldTypeFromName(publicFieldJson["Type"]);

						switch (publicFieldType)
						{
							case FieldType::Float:           publicField.SetStoredValue<float>(publicFieldJson["Value"]); break;
							case FieldType::Int:             publicField.SetStoredValue<int32_t>(publicFieldJson["Value"]); break;
							case FieldType::UnsignedInt:     publicField.SetStoredValue<uint32_t>(publicFieldJson["Value"]); break;
							case FieldType::String:          publicField.SetStoredValue<const std::string&>(publicFieldJson["Value"]); break;
							case FieldType::Vec2:
							{
								publicField.SetStoredValue<glm::vec2>({ publicFieldJson["Value"][0], publicFieldJson["Value"][1] });
								break;
							}
							case FieldType::Vec3:
							{
								publicField.SetStoredValue<glm::vec3>({ publicFieldJson["Value"][0], publicFieldJson["Value"][1], publicFieldJson["Value"][2] });
								break;
							}
							case FieldType::Vec4:
							{
								publicField.SetStoredValue<glm::vec4>({ publicFieldJson["Value"][0], publicFieldJson["Value"][1], publicFieldJson["Value"][2], publicFieldJson["Value"][3] });
								break;
							}
							case FieldType::ClassReference: break;// TODO: Not sure if storing a class reference would even make any sense, because eventually the pointer address will differ
							case FieldType::Asset: break; // TODO: Add assets (prefabs in our case)

							case FieldType::Entity:
							{
								publicField.SetStoredValue<UUID>(UUID(publicFieldJson["Value"]));
								break;
							}

							case FieldType::None:
							default: FROST_ASSERT_MSG("Field Type is not valid!");
						}
					}

				}
			}


		}
	}

	static std::string GetNameFromFilepath(const std::string& filepath)
	{
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind(".");
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;

		return filepath.substr(lastSlash, count);
	}


	static std::string GetNameFromFieldType(FieldType type)
	{
		switch (type)
		{
			case FieldType::Float: return "Float";
			case FieldType::Int: return "Int";
			case FieldType::UnsignedInt: return "UnsignedInt";
			case FieldType::String: return "String";
			case FieldType::Vec2: return "Vec2";
			case FieldType::Vec3: return "Vec3";
			case FieldType::Vec4: return "Vec4";
			case FieldType::ClassReference: return "ClassReference";
			case FieldType::Asset: return "Asset";
			case FieldType::Entity: return "Entity";

			case FieldType::None: FROST_ASSERT_MSG("Field Type is not valid!");
			default: FROST_ASSERT_MSG("Field Type is not valid!");
		}
		return "";
	}

	static FieldType GetFieldTypeFromName(const std::string& fieldTypeStr)
	{
		if     (fieldTypeStr == "Float" )           return FieldType::Float;
		else if(fieldTypeStr == "Int" )             return FieldType::Int;
		else if(fieldTypeStr == "UnsignedInt" )     return FieldType::UnsignedInt;
		else if(fieldTypeStr == "String" )          return FieldType::String;
		else if(fieldTypeStr == "Vec2" )            return FieldType::Vec2;
		else if(fieldTypeStr == "Vec3" )            return FieldType::Vec3;
		else if(fieldTypeStr == "Vec4" )            return FieldType::Vec4;
		else if(fieldTypeStr == "ClassReference" )  return FieldType::ClassReference;
		else if(fieldTypeStr == "Asset" )           return FieldType::Asset;
		else if(fieldTypeStr == "Entity" )          return FieldType::Entity;
		else FROST_ASSERT_MSG("Field Type is not valid!");

		return FieldType::None;
	}

}