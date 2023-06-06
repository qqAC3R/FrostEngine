#include "frostpch.h"
#include "ScriptInternalWrappers.h"

#include "Frost/Core/Input.h"

#include "ScriptEngine.h"
#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

#include "Frost/Physics/PhysicsEngine.h"
#include "Frost/Renderer/BindlessAllocator.h"

#include "Frost/Physics/PhysicsScene.h"
#include "Frost/Physics/PhysicsSettings.h"

#include "Frost/Physics/PhysX/PhysXScene.h"
#include "Frost/Physics/PhysX/PhysXShapes.h"

#include <mono/jit/jit.h>

namespace Frost
{
	// Get the global variables from "ScriptEngineRegistry.cpp.cpp"
	extern HashMap<MonoType*, std::function<bool(Entity&)>> s_HasComponentFuncs;
	extern HashMap<MonoType*, std::function<void(Entity&)>> s_CreateComponentFuncs;

	// Get the global variables from "BindlessAllocator.cpp"
	extern std::unordered_map<uint32_t, WeakRef<Texture2D>> m_TextureSlots;

namespace ScriptInternalCalls
{

	uint64_t EntityScript::GetParent(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");
		return entityMap.at(entityID).GetParent();
	}

	void EntityScript::SetParent(uint64_t entityID, uint64_t parentID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");
		Entity parent = entityMap.at(parentID);
		Entity child = entityMap.at(entityID);
		scene->ParentEntity(parent, child);
	}

	MonoArray* EntityScript::GetChildren(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		const auto& children = entity.GetChildren();

		auto klass = ScriptEngine::GetCoreClass("Frost.Entity");

		MonoArray* result = mono_array_new(mono_domain_get(), klass, children.size());

		uint32_t index = 0;
		for (auto child : children)
		{
			void* data[] = {
				&child
			};

			MonoObject* obj = ScriptEngine::ConstructClass("Frost.Entity:.ctor(ulong)", true, data);
			mono_array_set(result, MonoObject*, index++, obj);
		}

		return result;
	}

	void EntityScript::CreateComponent(uint64_t entityID, void* type)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		if (!s_HasComponentFuncs[monoType](entity))
		{
			s_CreateComponentFuncs[monoType](entity);
		}
		else
		{
			FROST_CORE_WARN("Component ({0}) is already initialized!", (void*)monoType);
		}
	}

	bool EntityScript::HasComponent(uint64_t entityID, void* type)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
		return s_HasComponentFuncs[monoType](entity);
	}

	uint64_t EntityScript::FindEntityByTag(MonoString* tag)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");

		Entity entity = scene->FindEntityByTag(mono_string_to_utf8(tag));
		if (entity)
			return entity.GetComponent<IDComponent>().ID;

		return 0;
	}

	uint64_t EntityScript::CreateEntity()
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");

		Entity result = scene->CreateEntity("Unnamed from C#");
		return result.GetUUID();
	}

	void EntityScript::DestroyEntity(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		scene->SubmitToDestroyEntity(entity);
	}

	MonoString* Components::Tag::GetTag(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& tagComponent = entity.GetComponent<TagComponent>();
		return mono_string_new(mono_domain_get(), tagComponent.Tag.c_str());
	}

	void Components::Tag::SetTag(uint64_t entityID, MonoString* tag)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& tagComponent = entity.GetComponent<TagComponent>();
		tagComponent.Tag = mono_string_to_utf8(tag);
	}

	void Components::Transform::SetTransform(uint64_t entityID, TransformComponent* inTransform)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<TransformComponent>() = *inTransform;
	}

	void Components::Transform::GetTransform(uint64_t entityID, TransformComponent* outTransform)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outTransform = entity.GetComponent<TransformComponent>();
	}

	void Components::Transform::SetTranslation(uint64_t entityID, glm::vec3* inTranslation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<TransformComponent>().Translation = *inTranslation;

		if (entity.HasComponent<RigidBodyComponent>())
		{
			auto& actor = PhysicsEngine::GetScene()->GetActor(entity);
			actor->SetTranslation(*inTranslation);
		}
	}

	void Components::Transform::GetTranslation(uint64_t entityID, glm::vec3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outTranslation = entity.GetComponent<TransformComponent>().Translation;
	}

	void Components::Transform::SetRotation(uint64_t entityID, glm::vec3* inRotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<TransformComponent>().Rotation = *inRotation;

		if (entity.HasComponent<RigidBodyComponent>())
		{
			auto& actor = PhysicsEngine::GetScene()->GetActor(entity);
			actor->SetRotation(*inRotation);
		}
	}

	void Components::Transform::GetRotation(uint64_t entityID, glm::vec3* outRotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outRotation = entity.GetComponent<TransformComponent>().Rotation;
	}

	void Components::Transform::SetScale(uint64_t entityID, glm::vec3* inScale)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<TransformComponent>().Scale = *inScale;
	}

	void Components::Transform::GetScale(uint64_t entityID, glm::vec3* outScale)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outScale = entity.GetComponent<TransformComponent>().Scale;
	}

	void Components::Transform::GetWorldSpaceTransform(uint64_t entityID, TransformComponent* outTransform)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outTransform = scene->GetTransformFromEntityAndParent(entity);
	}

	void Components::Camera::SetFOV(uint64_t entityID, float fov)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<CameraComponent>().Camera->SetFOV(fov);
		entity.GetComponent<CameraComponent>().Camera->RecalculateProjectionMatrix();
	}

	float Components::Camera::GetFOV(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<CameraComponent>().Camera->GetCameraFOV();
	}

	void Components::Camera::SetNearClip(uint64_t entityID, float nearClip)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<CameraComponent>().Camera->SetNearClip(nearClip);
		entity.GetComponent<CameraComponent>().Camera->RecalculateProjectionMatrix();
	}

	float Components::Camera::GetNearClip(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<CameraComponent>().Camera->GetNearClip();
	}

	void Components::Camera::SetFarClip(uint64_t entityID, float farClip)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		entity.GetComponent<CameraComponent>().Camera->SetFarClip(farClip);
		entity.GetComponent<CameraComponent>().Camera->RecalculateProjectionMatrix();
	}

	float Components::Camera::GetFarClip(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<CameraComponent>().Camera->GetFarClip();
	}

	void* RendererScript::Texture::Texture2DConstructor(uint32_t width, uint32_t height)
	{
		TextureSpecification textureSpecs{};
		textureSpecs.Usage = ImageUsage::ReadOnly;
		textureSpecs.Format = ImageFormat::RGBA8;
		textureSpecs.Sampler.SamplerWrap = ImageWrap::Repeat;
		textureSpecs.Sampler.SamplerFilter = ImageFilter::Linear;
		
		auto result = Texture2D::Create(width, height, textureSpecs);
		return new Texture2DPointer(result);
		//return new Ref<Texture2D>(result);
	}

	void* RendererScript::Texture::Texture2DWithFilepathConstructor(MonoString* filepath)
	{
		TextureSpecification textureSpecs{};
		textureSpecs.Usage = ImageUsage::ReadOnly;
		textureSpecs.Format = ImageFormat::RGBA8;
		textureSpecs.Sampler.SamplerWrap = ImageWrap::Repeat;
		textureSpecs.Sampler.SamplerFilter = ImageFilter::Linear;

		auto result = Texture2D::Create(mono_string_to_utf8(filepath), textureSpecs);
		return new Texture2DPointer(result);
		//return new Ref<Texture2D>(result);
	}

	void RendererScript::Texture::Texture2DDestructor(Texture2DPointer* _this)
	{
		delete _this;
	}

	void RendererScript::Texture::SetDataTexture2D(Texture2DPointer* _this, MonoArray* inData, int32_t count)
	{
		Ref<Texture2D> instance = _this->Texture;

		Buffer buffer = instance->GetWritableBuffer();

		uint32_t dataSize = count * sizeof(glm::vec4) / 4;
		FROST_ASSERT(bool(dataSize <= buffer.Size), "Buffer overflow! The buffer sent has a larger size than the one in the texture!");

		uint8_t* pixels = (uint8_t*)buffer.Data;
		uint32_t index = 0;
		for (uint32_t i = 0; i < instance->GetWidth() * instance->GetHeight(); i++)
		{
			glm::vec4& value = mono_array_get(inData, glm::vec4, i);
			*pixels++ = (uint32_t)(value.x * 255.0f);
			*pixels++ = (uint32_t)(value.y * 255.0f);
			*pixels++ = (uint32_t)(value.z * 255.0f);
			*pixels++ = (uint32_t)(value.w * 255.0f);
		}

		// TODO: When submitting to the GPU, we are creating everytime
		// a new staging buffer, which can be quite slow,
		// so instead we should just cache one and use it all the time
		instance->SubmitDataToGPU();
	}

	void RendererScript::Material::Destructor(MaterialMeshPointer* _this)
	{
		// This constructor shouldn't do anything because the destruction of materials is automatically done by the mesh class
		delete _this;
	}

	void RendererScript::Material::GetAbledoColor(MaterialMeshPointer* material, glm::vec3* outAlbedo)
	{
		//*outAlbedo = material->InternalMaterial->Get<glm::vec3>("AlbedoColor");
		*outAlbedo = glm::vec3(material->InternalMaterial->GetAlbedoColor());
	}

	void RendererScript::Material::SetAbledoColor(MaterialMeshPointer* material, glm::vec3* inAlbedo)
	{
		//material->InternalMaterial->Set<glm::vec3>("AlbedoColor", *inAlbedo);
		material->InternalMaterial->SetAlbedoColor(glm::vec4(*inAlbedo, 1.0f));
	}

	void RendererScript::Material::GetMetalness(MaterialMeshPointer* material, float outMetalness)
	{
		//outMetalness = material->InternalMaterial->Get<float>("MetalnessFactor");
		outMetalness = material->InternalMaterial->GetMetalness();
	}

	void RendererScript::Material::SetMetalness(MaterialMeshPointer* material, float inMetalness)
	{
		inMetalness = glm::clamp(inMetalness, 0.0f, 1.0f); // Clamp the values, so we avoid some weird artifacts/shader crashes
		//material->InternalMaterial->Set<float>("MetalnessFactor", inMetalness);
		material->InternalMaterial->SetMetalness(inMetalness);
	}

	void RendererScript::Material::GetRoughness(MaterialMeshPointer* material, float outRoughness)
	{
		outRoughness = material->InternalMaterial->GetRoughness();
	}

	void RendererScript::Material::SetRoughness(MaterialMeshPointer* material, float inRoughness)
	{
		inRoughness = glm::clamp(inRoughness, 0.0f, 1.0f); // Clamp the values, so we avoid some weird artifacts/shader crashes
		//material->InternalMaterial->Set<float>("RoughnessFactor", inRoughness);
		material->InternalMaterial->SetRoughness(inRoughness);
	}

	void RendererScript::Material::GetEmission(MaterialMeshPointer* material, float outEmission)
	{
		//outEmission = material->InternalMaterial->Get<float>("EmissionFactor");
		outEmission = material->InternalMaterial->GetEmission();
	}

	void RendererScript::Material::SetEmission(MaterialMeshPointer* material, float inEmission)
	{
		//material->InternalMaterial->Set<float>("EmissionFactor", inEmission);
		material->InternalMaterial->SetEmission(inEmission);
	}

	void* RendererScript::Material::GetTextureByString(MaterialMeshPointer* material, MonoString* textureType)
	{
		//Ref<DataStorage>& instance = *material;
		std::string textureTypeName = mono_string_to_utf8(textureType);

		//uint32_t textureId = 0;
		Ref<Texture2D> texture = nullptr;
		if      (textureTypeName == "Albedo")    { texture = material->InternalMaterial->GetAlbedoMap(); }
		else if (textureTypeName == "Roughness") { texture = material->InternalMaterial->GetRoughnessMap(); }
		else if (textureTypeName == "Metalness") { texture = material->InternalMaterial->GetMetalnessMap(); }
		else if (textureTypeName == "Normal")    { texture = material->InternalMaterial->GetNormalMap(); }

		return new Texture2DPointer(texture);

		//return new Texture2DPointer(m_TextureSlots[textureId].AsRef<Texture2D>());

		//return new Ref<Texture2D>(m_TextureSlots[textureId].Raw());
		//return m_TextureSlots[textureId].AsRef<Texture2D>();
	}

	void RendererScript::Material::SetTextureByString(MaterialMeshPointer* material, MonoString* textureType, Texture2DPointer* inTexturePtr)
	{
		std::string textureTypeName = mono_string_to_utf8(textureType);

		uint32_t textureId = 0;
		if (textureTypeName == "Albedo")         { material->InternalMaterial->SetAlbedoMap(inTexturePtr->Texture); }
		else if (textureTypeName == "Roughness") { material->InternalMaterial->SetRoughnessMap(inTexturePtr->Texture); }
		else if (textureTypeName == "Metalness") { material->InternalMaterial->SetMetalnessMap(inTexturePtr->Texture); }
		else if (textureTypeName == "Normal")    { material->InternalMaterial->SetNormalMap(inTexturePtr->Texture); }

#if 0
		if (inTexturePtr->Texture)
		{
			BindlessAllocator::AddTextureCustomSlot(inTexturePtr->Texture, textureId);
			material->GetMeshTextureTable()[textureId] = inTexturePtr->Texture;
		}
#endif
	}

	void* Components::Mesh::GetMeshPtr(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		if (entity.HasComponent<Frost::MeshComponent>())
		{
			auto mesh = entity.GetComponent<Frost::MeshComponent>().Mesh;
			return new MeshPointer(mesh);
			//return new Ref<Frost::Mesh>(mesh);
		}

		FROST_CORE_ERROR("Entity with ID: {0} doesn't have Mesh Component", entityID);
		return nullptr;
	}

	void Components::Mesh::SetMesh(uint64_t entityID, MaterialMeshPointer* meshPtr)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		if (entity.HasComponent<Frost::MeshComponent>())
		{
			entity.GetComponent<Frost::MeshComponent>().Mesh = meshPtr->Mesh;
		}
		else
		{
			FROST_CORE_ERROR("Entity with ID: {0} doesn't have Mesh Component", entityID);
		}
	}

	bool Components::Mesh::HasMaterial(uint64_t entityID, int index)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		if (entity.HasComponent<Frost::MeshComponent>())
		{
			auto mesh = entity.GetComponent<Frost::MeshComponent>().Mesh;
			return (index < mesh->GetMaterialCount());
		}

		FROST_CORE_ERROR("Entity with ID: {0} doesn't have Mesh Component", entityID);
		return false;
	}

	void* Components::Mesh::GetMaterial(uint64_t entityID, int index)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		if (entity.HasComponent<Frost::MeshComponent>())
		{
			auto mesh = entity.GetComponent<Frost::MeshComponent>().Mesh;
			if (index < mesh->GetMaterialCount())
			{
				//return new Ref<Frost::DataStorage>(&mesh->GetMaterialData(index));
				//return &mesh->GetMaterialData(index);
				return new MaterialMeshPointer(mesh, mesh->GetMaterialAsset(index));
			}
		}

		FROST_ASSERT_MSG("Material not found!");
		return nullptr;
	}

	void* RendererScript::Mesh::Constructor(MonoString* filepath)
	{
		std::string filepathStr = mono_string_to_utf8(filepath);
		Ref<Frost::MeshAsset> mesh = Frost::MeshAsset::Load(filepathStr);
		return new MeshPointer(mesh);
	}

	void RendererScript::Mesh::Destructor(MeshPointer* _this)
	{
		delete _this;
	}

	uint32_t RendererScript::Mesh::GetMaterialCount(MeshPointer* _this)
	{
		return _this->Mesh->GetMaterialCount();
	}

	void* RendererScript::Mesh::GetMaterialByIndex(MeshPointer* _this, int index)
	{
		auto mesh = _this->Mesh;
		if (index < mesh->GetMaterialCount())
		{
			return new MaterialMeshPointer(mesh, mesh->GetMaterialAsset(index));
		}

		FROST_ASSERT_MSG("Material not found!");
		return nullptr;
	}

	Frost::ScriptComponent Components::Script::GetInstance(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		if (entity.HasComponent<Frost::ScriptComponent>())
		{
			return entity.GetComponent<Frost::ScriptComponent>();
		}
		else
		{
			FROST_CORE_ERROR("Entity with ID: {0} doesn't have Script Component", entityID);
		}
		return ScriptComponent();
	}

	Frost::RigidBodyComponent::Type Components::Physics::RigidBody::GetBodyType(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		if (entity.HasComponent<Frost::RigidBodyComponent>())
		{
			return entity.GetComponent<Frost::RigidBodyComponent>().BodyType;
		}
		else
		{
			FROST_CORE_ERROR("Entity with ID: {0} doesn't have Rigid Body Component", entityID);
		}
		return Frost::RigidBodyComponent::Type();
	}

	void Components::Physics::RigidBody::SetTranslation(uint64_t entityID, glm::vec3* inTranslation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetTranslation(*inTranslation);
	}

	void Components::Physics::RigidBody::GetTranslation(uint64_t entityID, glm::vec3* outTranslation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		*outTranslation = actor->GetTranslation();
	}

	void Components::Physics::RigidBody::SetRotation(uint64_t entityID, glm::vec3* inRotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetRotation(*inRotation);
	}

	void Components::Physics::RigidBody::GetRotation(uint64_t entityID, glm::vec3* outRotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		*outRotation = actor->GetRotation();
	}

	void Components::Physics::RigidBody::SetMass(uint64_t entityID, float mass)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetMass(mass);
	}

	float Components::Physics::RigidBody::GetMass(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		return actor->GetMass();
	}

	void Components::Physics::RigidBody::SetLinearVelocity(uint64_t entityID, glm::vec3* velocity)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetLinearVelocity(*velocity);
	}

	void Components::Physics::RigidBody::GetLinearVelocity(uint64_t entityID, glm::vec3* velocity)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		*velocity = actor->GetLinearVelocity();
	}

	void Components::Physics::RigidBody::SetAngularVelocity(uint64_t entityID, glm::vec3* velocity)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetAngularVelocity(*velocity);
	}

	void Components::Physics::RigidBody::GetAngularVelocity(uint64_t entityID, glm::vec3* velocity)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		*velocity = actor->GetAngularVelocity();
	}

	void Components::Physics::RigidBody::SetMaxLinearVelocity(uint64_t entityID, float maxVelocity)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetMaxLinearVelocity(maxVelocity);
	}

	float Components::Physics::RigidBody::GetMaxLinearVelocity(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		return actor->GetMaxLinearVelocity();
	}

	void Components::Physics::RigidBody::SetMaxAngularVelocity(uint64_t entityID, float maxVelocity)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetMaxAngularVelocity(maxVelocity);
	}

	float Components::Physics::RigidBody::GetMaxAngularVelocity(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		return actor->GetMaxAngularVelocity();
	}

	void Components::Physics::RigidBody::GetKinematicTarget(uint64_t entityID, glm::vec3* outTargetPosition, glm::vec3* outTargetRotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		*outTargetPosition = actor->GetKinematicTargetPosition();
		*outTargetRotation = actor->GetKinematicTargetRotation();
	}

	void Components::Physics::RigidBody::SetKinematicTarget(uint64_t entityID, glm::vec3* inTargetPosition, glm::vec3* inTargetRotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetKinematicTarget(*inTargetPosition, *inTargetRotation);
	}

	void Components::Physics::RigidBody::AddForce(uint64_t entityID, glm::vec3* force, ForceMode forceMode)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->AddForce(*force, forceMode);
	}

	void Components::Physics::RigidBody::AddTorque(uint64_t entityID, glm::vec3* torque, ForceMode forceMode)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->AddForce(*torque, forceMode);
	}

	void Components::Physics::RigidBody::Rotate(uint64_t entityID, glm::vec3* rotation)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->Rotate(*rotation);
	}

	void Components::Physics::RigidBody::SetLockFlag(uint64_t entityID, ActorLockFlag flag, bool value)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		auto& component = entity.GetComponent<RigidBodyComponent>();
		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		actor->SetLockFlag(flag, value);
	}

	bool Components::Physics::RigidBody::IsLockFlagSet(uint64_t entityID, ActorLockFlag flag)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		auto& component = entity.GetComponent<RigidBodyComponent>();
		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		return actor->IsLockFlagSet(flag);
	}

	uint32_t Components::Physics::RigidBody::GetLockFlags(uint64_t entityID, ActorLockFlag flag, bool value)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		auto& component = entity.GetComponent<RigidBodyComponent>();
		Ref<PhysicsActor> actor = Frost::PhysicsEngine::GetScene()->GetActor(entity);
		return actor->GetLockFlags();
	}

	uint32_t Components::Physics::RigidBody::GetLayer(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		FROST_ASSERT_INTERNAL(entity.HasComponent<RigidBodyComponent>());

		return entity.GetComponent<RigidBodyComponent>().Layer;
	}

	bool Input::IsKeyPressed(Key::KeyCode keycode)
	{
		return Frost::Input::IsKeyPressed(keycode);
	}

	bool Input::IsMouseButtonPressed(Mouse::MouseCode button)
	{
		return Frost::Input::IsMouseButtonPressed(button);
	}

	void Input::GetMousePosition(glm::vec2* position)
	{
		auto pos = Frost::Input::GetMousePosition();
		*position = { pos.first, pos.second };
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		Frost::Input::SetCursorMode(mode);
	}

	Frost::CursorMode Input::GetCursorMode()
	{
		return Frost::Input::GetCursorMode();
	}

	void Log::LogMessage(LogLevel level, MonoString* message)
	{
		char* msg = mono_string_to_utf8(message);
		switch (level)
		{
		case LogLevel::Trace:
			FROST_TRACE(msg);
			break;
		case LogLevel::Debug:
			FROST_INFO(msg);
			break;
		case LogLevel::Info:
			FROST_INFO(msg);
			break;
		case LogLevel::Warn:
			FROST_WARN(msg);
			break;
		case LogLevel::Error:
			FROST_ERROR(msg);
			break;
		case LogLevel::Critical:
			FROST_CRITICAL(msg);
			break;
		}
	}

	bool Components::Physics::Raycast(RaycastData* inData, RaycastHit* hit)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		FROST_ASSERT(PhysicsEngine::GetScene()->IsValid(), "No active scene!");

		RaycastHit temp;
		bool success = PhysicsEngine::GetScene()->Raycast(inData->Origin, inData->Direction, inData->MaxDistance, &temp);

		if (success && inData->RequiredComponentTypes != nullptr)
		{
			Entity entity = scene->FindEntityByUUID(temp.HitEntity);
			size_t length = mono_array_length(inData->RequiredComponentTypes);

			for (size_t i = 0; i < length; i++)
			{
				void* rawType = mono_array_get(inData->RequiredComponentTypes, void*, i);
				if (rawType == nullptr)
				{
					FROST_CORE_ERROR("Why did you feel the need to pass a \"null\" System.Type to Physics.Raycast?");
					success = false;
					break;
				}

				MonoType* type = mono_reflection_type_get_type((MonoReflectionType*)rawType);

				if (!s_HasComponentFuncs[type](entity))
				{
					success = false;
					break;
				}
			}
		}

		if (!success)
		{
			temp.HitEntity = 0;
			temp.Distance = 0.0f;
			temp.Normal = glm::vec3(0.0f);
			temp.Position = glm::vec3(0.0f);
		}

		*hit = temp;
		return success;

	}

	void Components::Physics::GetGravity(glm::vec3* outGravity)
	{
		Ref<PhysicsScene> scene = PhysicsEngine::GetScene();
		FROST_ASSERT(scene && scene->IsValid(), "No active Phyiscs Scene!");
		*outGravity = scene->GetGravity();
	}

	void Components::Physics::SetGravity(glm::vec3* inGravity)
	{
		Ref<PhysicsScene> scene = PhysicsEngine::GetScene();
		FROST_ASSERT(scene && scene->IsValid(), "No active Phyiscs Scene!");
		scene->SetGravity(*inGravity);
	}


	// Helper function for the Overlap functions below
	static void AddPhysXCollidersToArray(MonoArray* array, const std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS>& hits, uint32_t count, uint32_t arrayLength)
	{
		uint32_t arrayIndex = 0;
		for (uint32_t i = 0; i < count; i++)
		{
			Entity& entity = *(Entity*)hits[i].actor->userData;

			if (entity.HasComponent<BoxColliderComponent>() && arrayIndex < arrayLength)
			{
				auto& boxCollider = entity.GetComponent<BoxColliderComponent>();

				void* data[] = {
					&entity.GetUUID(),
					&boxCollider.IsTrigger,
					&boxCollider.Size,
					&boxCollider.Offset
				};

				MonoObject* obj = ScriptEngine::ConstructClass("Frost.BoxCollider:.ctor(ulong,bool,Vector3,Vector3)", true, data);
				mono_array_set(array, MonoObject*, arrayIndex++, obj);
			}

			if (entity.HasComponent<SphereColliderComponent>() && arrayIndex < arrayLength)
			{
				auto& sphereCollider = entity.GetComponent<SphereColliderComponent>();

				void* data[] = {
					&entity.GetUUID(),
					&sphereCollider.IsTrigger,
					&sphereCollider.Radius
				};

				MonoObject* obj = ScriptEngine::ConstructClass("Frost.SphereCollider:.ctor(ulong,bool,single)", true, data);
				mono_array_set(array, MonoObject*, arrayIndex++, obj);
			}

			if (entity.HasComponent<CapsuleColliderComponent>() && arrayIndex < arrayLength)
			{
				auto& capsuleCollider = entity.GetComponent<CapsuleColliderComponent>();

				void* data[] = {
					&entity.GetUUID(),
					&capsuleCollider.IsTrigger,
					&capsuleCollider.Radius,
					&capsuleCollider.Height
				};

				MonoObject* obj = ScriptEngine::ConstructClass("Frost.CapsuleCollider:.ctor(ulong,bool,single,single)", true, data);
				mono_array_set(array, MonoObject*, arrayIndex++, obj);
			}

			if (entity.HasComponent<MeshColliderComponent>() && arrayIndex < arrayLength)
			{
				auto& meshCollider = entity.GetComponent<MeshColliderComponent>();

				//Ref<Mesh>* mesh = new Ref<Mesh>(AssetManager::GetAsset<Mesh>(meshCollider.CollisionMesh));
				MeshPointer* meshPointer = new MeshPointer(meshCollider.CollisionMesh);
				void* data[] = {
					&entity.GetUUID(),
					&meshCollider.IsTrigger,
					&meshPointer
				};

				MonoObject* obj = ScriptEngine::ConstructClass("Frost.MeshCollider:.ctor(ulong,bool,intptr)", true, data);
				mono_array_set(array, MonoObject*, arrayIndex++, obj);
			}

		}
	}

	static std::array<physx::PxOverlapHit, OVERLAP_MAX_COLLIDERS> s_OverlapBuffer;

	MonoArray* Components::Physics::OverlapBox(glm::vec3* origin, glm::vec3* halfSize)
	{
		FROST_ASSERT_INTERNAL(PhysicsEngine::GetScene()->IsValid());

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{

				MonoArray* outColliders = nullptr;
				memset(s_OverlapBuffer.data(), 0, OVERLAP_MAX_COLLIDERS * sizeof(physx::PxOverlapHit));

				uint32_t count;
				Ref<PhysXScene> physxScene = PhysicsEngine::GetScene().As<PhysXScene>();
				if (physxScene->OverlapBox(*origin, *halfSize, s_OverlapBuffer, count))
				{
					outColliders = mono_array_new(mono_domain_get(), ScriptEngine::GetCoreClass("Frost.Collider"), count);
					AddPhysXCollidersToArray(outColliders, s_OverlapBuffer, count, count);
				}
				return outColliders;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return nullptr;
			}
		}


		return nullptr;
	}

	MonoArray* Components::Physics::OverlapCapsule(glm::vec3* origin, float radius, float halfHeight)
	{
		FROST_ASSERT_INTERNAL(PhysicsEngine::GetScene()->IsValid());

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{

				MonoArray* outColliders = nullptr;
				memset(s_OverlapBuffer.data(), 0, OVERLAP_MAX_COLLIDERS * sizeof(physx::PxOverlapHit));

				uint32_t count;
				Ref<PhysXScene> physxScene = PhysicsEngine::GetScene().As<PhysXScene>();
				if (physxScene->OverlapCapsule(*origin, radius, halfHeight, s_OverlapBuffer, count))
				{
					outColliders = mono_array_new(mono_domain_get(), ScriptEngine::GetCoreClass("Frost.Collider"), count);
					AddPhysXCollidersToArray(outColliders, s_OverlapBuffer, count, count);
				}
				return outColliders;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return nullptr;
			}
		}

		return nullptr;
	}

	MonoArray* Components::Physics::OverlapSphere(glm::vec3* origin, float radius)
	{
		FROST_ASSERT_INTERNAL(PhysicsEngine::GetScene()->IsValid());

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{

				MonoArray* outColliders = nullptr;
				memset(s_OverlapBuffer.data(), 0, OVERLAP_MAX_COLLIDERS * sizeof(physx::PxOverlapHit));

				uint32_t count;
				Ref<PhysXScene> physxScene = PhysicsEngine::GetScene().As<PhysXScene>();
				if (physxScene->OverlapSphere(*origin, radius, s_OverlapBuffer, count))
				{
					outColliders = mono_array_new(mono_domain_get(), ScriptEngine::GetCoreClass("Frost.Collider"), count);
					AddPhysXCollidersToArray(outColliders, s_OverlapBuffer, count, count);
				}
				return outColliders;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return nullptr;
			}
		}

		return nullptr;
	}

	void Components::Physics::BoxCollider::SetSize(uint64_t entityID, glm::vec3* inSize)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& boxColliderComponent = entity.GetComponent<BoxColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				boxColliderComponent.ColliderHandle.As<PhysX::BoxColliderShape>()->SetSize(entity.Transform().Scale, *inSize);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void Components::Physics::BoxCollider::GetSize(uint64_t entityID, glm::vec3* outSize)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outSize = entity.GetComponent<BoxColliderComponent>().Size;
	}

	void Components::Physics::BoxCollider::SetOffset(uint64_t entityID, glm::vec3* inOffset)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& boxColliderComponent = entity.GetComponent<BoxColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				boxColliderComponent.ColliderHandle.As<PhysX::BoxColliderShape>()->SetOffset(*inOffset);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void Components::Physics::BoxCollider::GetOffset(uint64_t entityID, glm::vec3* outOffset)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outOffset = entity.GetComponent<BoxColliderComponent>().Offset;
	}

	bool Components::Physics::BoxCollider::GetIsTrigger(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<BoxColliderComponent>().IsTrigger;
	}

	void Components::Physics::BoxCollider::SetIsTrigger(uint64_t entityID, bool isTrigger)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& boxColliderComponent = entity.GetComponent<BoxColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				boxColliderComponent.ColliderHandle.As<PhysX::BoxColliderShape>()->SetTrigger(isTrigger);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void Components::Physics::SphereCollider::SetRadius(uint64_t entityID, float radius)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& sphereColliderComponent = entity.GetComponent<SphereColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				sphereColliderComponent.ColliderHandle.As<PhysX::SphereColliderShape>()->SetRadius(entity.Transform().Scale, radius);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	float Components::Physics::SphereCollider::GetRadius(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<SphereColliderComponent>().Radius;
	}
	

	void Components::Physics::SphereCollider::SetOffset(uint64_t entityID, glm::vec3* inOffset)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& sphereColliderComponent = entity.GetComponent<SphereColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				sphereColliderComponent.ColliderHandle.As<PhysX::SphereColliderShape>()->SetOffset(*inOffset);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void Components::Physics::SphereCollider::GetOffset(uint64_t entityID, glm::vec3* outOffset)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outOffset = entity.GetComponent<SphereColliderComponent>().Offset;
	}

	bool Components::Physics::SphereCollider::GetIsTrigger(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<SphereColliderComponent>().IsTrigger;
	}

	void Components::Physics::SphereCollider::SetIsTrigger(uint64_t entityID, bool isTrigger)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& sphereColliderComponent = entity.GetComponent<SphereColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				sphereColliderComponent.ColliderHandle.As<PhysX::SphereColliderShape>()->SetTrigger(isTrigger);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void Components::Physics::CapsuleCollider::SetRadius(uint64_t entityID, float radius)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				capsuleColliderComponent.ColliderHandle.As<PhysX::CapsuleColliderShape>()->SetRadius(entity.Transform().Scale, radius);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	float Components::Physics::CapsuleCollider::GetRadius(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<CapsuleColliderComponent>().Radius;
	}

	void Components::Physics::CapsuleCollider::SetHeight(uint64_t entityID, float height)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				capsuleColliderComponent.ColliderHandle.As<PhysX::CapsuleColliderShape>()->SetHeight(entity.Transform().Scale, height);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	float Components::Physics::CapsuleCollider::GetHeight(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<CapsuleColliderComponent>().Height;
	}

	void Components::Physics::CapsuleCollider::SetOffset(uint64_t entityID, glm::vec3* inOffset)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				capsuleColliderComponent.ColliderHandle.As<PhysX::CapsuleColliderShape>()->SetOffset(*inOffset);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void Components::Physics::CapsuleCollider::GetOffset(uint64_t entityID, glm::vec3* outOffset)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		*outOffset = entity.GetComponent<CapsuleColliderComponent>().Offset;
	}

	bool Components::Physics::CapsuleCollider::GetIsTrigger(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<CapsuleColliderComponent>().IsTrigger;
	}

	void Components::Physics::CapsuleCollider::SetIsTrigger(uint64_t entityID, bool isTrigger)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& capsuleColliderComponent = entity.GetComponent<CapsuleColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				capsuleColliderComponent.ColliderHandle.As<PhysX::CapsuleColliderShape>()->SetTrigger(isTrigger);
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	void* Components::Physics::MeshCollider::GetColliderMesh(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return new MeshPointer(entity.GetComponent<MeshColliderComponent>().CollisionMesh);
	}

	bool Components::Physics::MeshCollider::IsConvex(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<MeshColliderComponent>().IsConvex;
	}

	void Components::Physics::MeshCollider::SetConvex(uint64_t entityID, bool isConvex)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& meshColliderComponent = entity.GetComponent<MeshColliderComponent>();
		auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();
		Ref<PhysicsActor> actor = PhysicsEngine::GetScene()->GetActor(entity);

		if (meshColliderComponent.IsConvex == isConvex) return;

		// After we checked if the boolean value is not the same, we can set the new value inside the component,
		// because we will need it for creating the physics shape
		meshColliderComponent.IsConvex = isConvex;

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				if (rigidBodyComponent.BodyType == RigidBodyComponent::Type::Dynamic && !isConvex)
				{
					FROST_CORE_WARN("[PhysXActor] Triangle meshes can't have a dynamic rigidbody!");
				}
				else
				{
					auto& colliders = actor->GetColliders();
					Ref<PhysXActor> physXActor = actor.As<PhysXActor>();

					for (auto& collider : colliders)
					{
						if (collider == meshColliderComponent.ColliderHandle)
						{
							collider->DetachFromActor(actor);

							if (meshColliderComponent.IsConvex)
								meshColliderComponent.ColliderHandle = Ref<PhysX::ConvexMeshShape>::Create(meshColliderComponent, *(physXActor.Raw()), entity, glm::vec3(0.0f));
							else
								meshColliderComponent.ColliderHandle = Ref<PhysX::TriangleMeshShape>::Create(meshColliderComponent, *(physXActor.Raw()), entity, glm::vec3(0.0f));

							collider = meshColliderComponent.ColliderHandle;
						}
					}

					if (colliders.size() == 0)
					{
						actor->AddCollider(meshColliderComponent, entity);
					}

					actor->SetSimulationData(rigidBodyComponent.Layer);
				}

				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	bool Components::Physics::MeshCollider::GetIsTrigger(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		return entity.GetComponent<MeshColliderComponent>().IsTrigger;
	}

	void Components::Physics::MeshCollider::SetIsTrigger(uint64_t entityID, bool isTrigger)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& meshColliderComponent = entity.GetComponent<MeshColliderComponent>();

		switch (PhysicsEngine::GetAPI())
		{
			case PhysicsEngine::API::PhysX:
			{
				if (meshColliderComponent.IsConvex)
					meshColliderComponent.ColliderHandle.As<PhysX::ConvexMeshShape>()->SetTrigger(isTrigger);
				else
					FROST_CORE_WARN("[PhysX] Triangle Mesh Colliders cannot be triggers!");
				break;
			}

			default:
			{
				FROST_ASSERT_MSG("The Physics Engine is not valid!");
				return;
			}
		}
	}

	MonoArray* Components::Animation::GetAnimations(uint64_t entityID)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& animationComponent = entity.GetComponent<AnimationComponent>();
		const Vector<Ref<Frost::Animation>>& animations = animationComponent.MeshComponentPtr->Mesh->GetMeshAsset()->GetAnimations();

		MonoArray* animationsMonoArray = mono_array_new(mono_domain_get(), mono_get_string_class(), animations.size());

		uint32_t index = 0;
		for (auto& animation : animations)
		{
			MonoString* animationName = mono_string_new(mono_domain_get(), animation->GetName().c_str());
			mono_array_set(animationsMonoArray, MonoString*, index, animationName);
			index++;
		}

		return animationsMonoArray;
	}

	void Components::Animation::SetActiveAnimation(uint64_t entityID, MonoString* animationName)
	{
		Scene* scene = ScriptEngine::GetCurrentSceneContext();
		FROST_ASSERT(scene, "No active scene!");
		const auto& entityMap = scene->GetEntityMap();
		FROST_ASSERT(bool(entityMap.find(entityID) != entityMap.end()), "Invalid entity ID or entity doesn't exist in scene!");

		Entity entity = entityMap.at(entityID);
		auto& animationComponent = entity.GetComponent<AnimationComponent>();
		const Vector<Ref<Frost::Animation>>& animations = animationComponent.MeshComponentPtr->Mesh->GetMeshAsset()->GetAnimations();

		std::string activeAnimationName = mono_string_to_utf8(animationName);

		uint32_t index = 0;
		for (auto& animation : animations)
		{
			if (animation->GetName() == activeAnimationName)
			{
				animationComponent.Controller->SetActiveAnimation(animation);
				break;
			}
		}
	}

} }
