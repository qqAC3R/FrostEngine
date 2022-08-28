#include "frostpch.h"
#include "SceneSerializer.h"

#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Renderer/Renderer.h"

#include "Entity.h"

namespace Frost
{

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{

	}

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		std::ofstream istream(filepath);
		istream.clear();


		nlohmann::ordered_json out;

		m_Scene->m_Registry.each([&](auto entityID)
		{
			Entity entity = { entityID, m_Scene.Raw() };
			if (!entity)
				return;

			SerializeEntity(out, entity);
		});

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
				meshFilepath = mesh->GetFilepath();
				for (uint32_t k = 0; k < mesh->GetMaterialCount(); k++)
				{
					// Setting up the material data into a storage buffer
					DataStorage& materialData = mesh->GetMaterialData(k);

					glm::vec4 albedoColor = materialData.Get<glm::vec4>("AlbedoColor");

					nlohmann::json material = nlohmann::json();
					material["AlbedoTextureFilepath"] = mesh->m_TexturesFilepaths[k].AlbedoFilepath;
					material["AlbedoColor"] = { albedoColor.r, albedoColor.g, albedoColor.b, albedoColor.a };

					material["NormalMapTextureFilepath"] = mesh->m_TexturesFilepaths[k].NormalMapFilepath;
					material["NormalMapUse"] = materialData.Get<uint32_t>("UseNormalMap");

					material["RoughnessTextureFilepath"] = mesh->m_TexturesFilepaths[k].RoughnessMapFilepath;
					material["RoughnessValue"] = materialData.Get<float>("RoughnessFactor");

					material["MetalnessTextureFilepath"] = mesh->m_TexturesFilepaths[k].MetalnessMapFilepath;
					material["MetalnessValue"] = materialData.Get<float>("MetalnessFactor");

					material["EmissionValue"] = materialData.Get<float>("EmissionFactor");

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
				Ref<Mesh> mesh = meshComponent.Mesh;

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

		

		out.push_back(entityOut);
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		std::string content;

		std::ifstream instream(filepath);
		
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

			Entity ent = m_Scene->CreateEntityWithID(uuid);
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

				Mesh::MeshBuildSettings meshBuildSettings{};
				meshBuildSettings.LoadMaterials = false;
				meshComponent.Mesh = Mesh::LoadCustomMesh(entity["MeshComponent"]["Filepath"], MaterialInstance(), meshBuildSettings);

				Ref<Mesh> mesh = meshComponent.Mesh;
				Ref<Texture2D> whiteTexture = Renderer::GetWhiteLUT();
				

				uint32_t materialCount = entity["MeshComponent"]["Materials"].size();

				mesh->m_Textures.reserve(materialCount);
				mesh->m_TexturesFilepaths.resize(materialCount);
				mesh->m_MaterialData.resize(materialCount);
				for (uint32_t i = 0; i < materialCount; i++)
				{
					nlohmann::json materialsIn = entity["MeshComponent"]["Materials"][i];

					// Albedo -         vec4        (16 bytes)
					// Roughness -      float       (4 bytes)
					// Metalness -      float       (4 bytes)
					// Emission -       float       (4 bytes)
					// UseNormalMap -   uint32_t    (4 bytes)
					// Texture IDs -    4 uint32_t  (16 bytes)
					mesh->m_MaterialData[i].Allocate(48);

					std::vector<float> albedo = materialsIn["AlbedoColor"];
					glm::vec4 albedoColor = { albedo[0], albedo[1], albedo[2], albedo[3] };

					// Fill up the data in the correct order for us to copy it later
					DataStorage& materialData = mesh->m_MaterialData[i];
					materialData.Add<glm::vec4>("AlbedoColor", albedoColor);
					materialData.Add<float>("EmissionFactor",  materialsIn["EmissionValue"]);
					materialData.Add<float>("RoughnessFactor", materialsIn["RoughnessValue"]);
					materialData.Add<float>("MetalnessFactor", materialsIn["MetalnessValue"]);

					materialData.Add<uint32_t>("UseNormalMap", materialsIn["NormalMapUse"]);

					materialData.Add("AlbedoTexture", 0);
					materialData.Add("RoughnessTexture", 0);
					materialData.Add("MetalnessTexture", 0);
					materialData.Add("NormalTexture", 0);

					// Each mesh has 4 textures, and se we allocated numMaterials * 4 texture slots.
					uint32_t albedoTextureIndex =    (i * 4) + 0;
					uint32_t roughnessTextureIndex = (i * 4) + 1;
					uint32_t metalnessTextureIndex = (i * 4) + 2;
					uint32_t normalMapTextureIndex = (i * 4) + 3;

					materialData.Set("AlbedoTexture", mesh->m_TextureAllocatorSlots[albedoTextureIndex]);
					materialData.Set("NormalTexture", mesh->m_TextureAllocatorSlots[normalMapTextureIndex]);
					materialData.Set("RoughnessTexture", mesh->m_TextureAllocatorSlots[roughnessTextureIndex]);
					materialData.Set("MetalnessTexture", mesh->m_TextureAllocatorSlots[metalnessTextureIndex]);


					// Creating the materials
					std::string albedoTextureFilepath = materialsIn["AlbedoTextureFilepath"];
					std::string normalTextureFilepath = materialsIn["NormalMapTextureFilepath"];
					std::string roughnessTextureFilepath = materialsIn["RoughnessTextureFilepath"];
					std::string metalnessTextureFilepath = materialsIn["MetalnessTextureFilepath"];

					// Albedo texture
					if (albedoTextureFilepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						textureSpec.UseMips = true;
						Ref<Texture2D> albedoTexture = Texture2D::Create(albedoTextureFilepath, textureSpec);
						if (albedoTexture->Loaded())
						{
							albedoTexture->GenerateMipMaps();

							uint32_t textureId = mesh->m_TextureAllocatorSlots[albedoTextureIndex];
							mesh->m_Textures[textureId] = albedoTexture;
							mesh->m_TexturesFilepaths[i].AlbedoFilepath = albedoTextureFilepath;

							VulkanBindlessAllocator::AddTextureCustomSlot(albedoTexture, textureId);
						}
						else
						{
							FROST_CORE_ERROR("Couldn't load albedo texture: {0}", albedoTextureFilepath);

							uint32_t textureId = mesh->m_TextureAllocatorSlots[albedoTextureIndex];
							mesh->m_Textures[textureId] = whiteTexture;

							VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
						}
					}
					else
					{
						uint32_t textureId = mesh->m_TextureAllocatorSlots[albedoTextureIndex];
						mesh->m_Textures[textureId] = whiteTexture;

						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}


					// Normal texture
					if (normalTextureFilepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						Ref<Texture2D> normalTexture = Texture2D::Create(normalTextureFilepath, textureSpec);
						if (normalTexture->Loaded())
						{
							uint32_t textureId = mesh->m_TextureAllocatorSlots[normalMapTextureIndex];
							mesh->m_Textures[textureId] = normalTexture;
							mesh->m_TexturesFilepaths[i].NormalMapFilepath = normalTextureFilepath;

							VulkanBindlessAllocator::AddTextureCustomSlot(normalTexture, textureId);
						}
						else
						{
							FROST_CORE_ERROR("Couldn't load normal texture: {0}", normalTextureFilepath);

							uint32_t textureId = mesh->m_TextureAllocatorSlots[normalMapTextureIndex];
							mesh->m_Textures[textureId] = whiteTexture;

							VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
						}
					}
					else
					{
						uint32_t textureId = mesh->m_TextureAllocatorSlots[normalMapTextureIndex];
						mesh->m_Textures[textureId] = whiteTexture;

						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}

					// Roughness texture
					if (roughnessTextureFilepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						Ref<Texture2D> roughnessTexture = Texture2D::Create(roughnessTextureFilepath, textureSpec);
						if (roughnessTexture->Loaded())
						{
							uint32_t textureId = mesh->m_TextureAllocatorSlots[roughnessTextureIndex];
							mesh->m_Textures[textureId] = roughnessTexture;
							mesh->m_TexturesFilepaths[i].RoughnessMapFilepath = roughnessTextureFilepath;

							VulkanBindlessAllocator::AddTextureCustomSlot(roughnessTexture, textureId);
						}
						else
						{
							FROST_CORE_ERROR("Couldn't load roughness texture: {0}", roughnessTextureFilepath);

							uint32_t textureId = mesh->m_TextureAllocatorSlots[roughnessTextureIndex];
							mesh->m_Textures[textureId] = whiteTexture;

							VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
						}
					}
					else
					{
						uint32_t textureId = mesh->m_TextureAllocatorSlots[roughnessTextureIndex];
						mesh->m_Textures[textureId] = whiteTexture;

						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}

					// Metalness texture
					if (metalnessTextureFilepath != "")
					{
						TextureSpecification textureSpec{};
						textureSpec.Format = ImageFormat::RGBA8;
						textureSpec.Usage = ImageUsage::ReadOnly;
						Ref<Texture2D> metalnessTexture = Texture2D::Create(metalnessTextureFilepath, textureSpec);
						if (metalnessTexture->Loaded())
						{
							uint32_t textureId = mesh->m_TextureAllocatorSlots[metalnessTextureIndex];
							mesh->m_Textures[textureId] = metalnessTexture;
							mesh->m_TexturesFilepaths[i].MetalnessMapFilepath = metalnessTextureFilepath;

							VulkanBindlessAllocator::AddTextureCustomSlot(metalnessTexture, textureId);
						}
						else
						{
							FROST_CORE_ERROR("Couldn't load roughness texture: {0}", metalnessTextureFilepath);

							uint32_t textureId = mesh->m_TextureAllocatorSlots[metalnessTextureIndex];
							mesh->m_Textures[textureId] = whiteTexture;

							VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
						}
					}
					else
					{
						uint32_t textureId = mesh->m_TextureAllocatorSlots[metalnessTextureIndex];
						mesh->m_Textures[textureId] = whiteTexture;

						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}
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
					Ref<Mesh> mesh = meshComponent.Mesh;

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


		}

		return false;
	}


}