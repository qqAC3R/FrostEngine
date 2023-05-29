#include "frostpch.h"
#include "ScriptEngineRegistry.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

#include "ScriptInternalWrappers.h"

#include "Frost/EntitySystem/Entity.h"
#include "Frost/EntitySystem/Components.h"

namespace Frost
{
	HashMap<MonoType*, std::function<bool(Entity&)>> s_HasComponentFuncs;
	HashMap<MonoType*, std::function<void(Entity&)>> s_CreateComponentFuncs;

	extern MonoImage* s_CoreAssemblyImage;

#define Component_RegisterType(Type) \
	{\
		MonoType* type = mono_reflection_type_from_name("Frost." #Type, s_CoreAssemblyImage);\
		if (type) {\
			s_HasComponentFuncs[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };\
			s_CreateComponentFuncs[type] = [](Entity& entity) { entity.AddComponent<Type>(); };\
		} else {\
			FROST_CORE_ERROR("No C# component class found for " #Type "!");\
		}\
	}

	static void InitComponentTypes()
	{
		Component_RegisterType(TagComponent);
		Component_RegisterType(TransformComponent);
		Component_RegisterType(MeshComponent);
		Component_RegisterType(CameraComponent);
		Component_RegisterType(ScriptComponent);
		Component_RegisterType(RigidBodyComponent);
		Component_RegisterType(BoxColliderComponent);
		Component_RegisterType(SphereColliderComponent);
		Component_RegisterType(CapsuleColliderComponent);
		Component_RegisterType(MeshColliderComponent);
		Component_RegisterType(AnimationComponent);
	}

	void ScriptEngineRegistry::RegisterAll()
	{
		// TODO: Add all wrapper classes firstly
		InitComponentTypes();

		// Log native methods
		mono_add_internal_call("Frost.Log::LogMessage_Native", Frost::ScriptInternalCalls::Log::LogMessage);

		// Entity class native methods
		mono_add_internal_call("Frost.Entity::GetParent_Native", Frost::ScriptInternalCalls::EntityScript::GetParent);
		mono_add_internal_call("Frost.Entity::SetParent_Native", Frost::ScriptInternalCalls::EntityScript::SetParent);
		mono_add_internal_call("Frost.Entity::GetChildren_Native", Frost::ScriptInternalCalls::EntityScript::GetChildren);
		mono_add_internal_call("Frost.Entity::CreateComponent_Native", Frost::ScriptInternalCalls::EntityScript::CreateComponent);
		mono_add_internal_call("Frost.Entity::HasComponent_Native", Frost::ScriptInternalCalls::EntityScript::HasComponent);
		mono_add_internal_call("Frost.Entity::FindEntityByTag_Native", Frost::ScriptInternalCalls::EntityScript::FindEntityByTag);
		mono_add_internal_call("Frost.Entity::CreateEntity_Native", Frost::ScriptInternalCalls::EntityScript::CreateEntity);
		
		// Tag Component class native methods
		mono_add_internal_call("Frost.TagComponent::GetTag_Native", Frost::ScriptInternalCalls::Components::Tag::GetTag);
		mono_add_internal_call("Frost.TagComponent::SetTag_Native", Frost::ScriptInternalCalls::Components::Tag::SetTag);

		// Camera Component class native methods
		mono_add_internal_call("Frost.CameraComponent::SetFOV_Native", Frost::ScriptInternalCalls::Components::Camera::SetFOV);
		mono_add_internal_call("Frost.CameraComponent::GetFOV_Native", Frost::ScriptInternalCalls::Components::Camera::GetFOV);
		mono_add_internal_call("Frost.CameraComponent::SetNearClip_Native", Frost::ScriptInternalCalls::Components::Camera::SetNearClip);
		mono_add_internal_call("Frost.CameraComponent::GetNearClip_Native", Frost::ScriptInternalCalls::Components::Camera::GetNearClip);
		mono_add_internal_call("Frost.CameraComponent::SetFarClip_Native", Frost::ScriptInternalCalls::Components::Camera::SetFarClip);
		mono_add_internal_call("Frost.CameraComponent::GetFarClip_Native", Frost::ScriptInternalCalls::Components::Camera::GetFarClip);

		// Transform Component class native methods
		mono_add_internal_call("Frost.TransformComponent::SetTransform_Native", Frost::ScriptInternalCalls::Components::Transform::SetTransform);
		mono_add_internal_call("Frost.TransformComponent::GetTransform_Native", Frost::ScriptInternalCalls::Components::Transform::GetTransform);
		mono_add_internal_call("Frost.TransformComponent::SetTranslation_Native", Frost::ScriptInternalCalls::Components::Transform::SetTranslation);
		mono_add_internal_call("Frost.TransformComponent::GetTranslation_Native", Frost::ScriptInternalCalls::Components::Transform::GetTranslation);
		mono_add_internal_call("Frost.TransformComponent::SetRotation_Native", Frost::ScriptInternalCalls::Components::Transform::SetRotation);
		mono_add_internal_call("Frost.TransformComponent::GetRotation_Native", Frost::ScriptInternalCalls::Components::Transform::GetRotation);
		mono_add_internal_call("Frost.TransformComponent::SetScale_Native", Frost::ScriptInternalCalls::Components::Transform::SetScale);
		mono_add_internal_call("Frost.TransformComponent::GetScale_Native", Frost::ScriptInternalCalls::Components::Transform::GetScale);
		mono_add_internal_call("Frost.TransformComponent::GetWorldSpaceTransform_Native", Frost::ScriptInternalCalls::Components::Transform::GetScale);

		// Texture2D class native methods
		mono_add_internal_call("Frost.Texture2D::Constructor_Native", Frost::ScriptInternalCalls::RendererScript::Texture::Texture2DConstructor);
		mono_add_internal_call("Frost.Texture2D::ConstructorWithFilepath_Native", Frost::ScriptInternalCalls::RendererScript::Texture::Texture2DWithFilepathConstructor);
		mono_add_internal_call("Frost.Texture2D::Destructor_Native", Frost::ScriptInternalCalls::RendererScript::Texture::Texture2DDestructor);
		mono_add_internal_call("Frost.Texture2D::SetData_Native", Frost::ScriptInternalCalls::RendererScript::Texture::SetDataTexture2D);

		// Material class native methods
		mono_add_internal_call("Frost.Material::Destructor_Native", Frost::ScriptInternalCalls::RendererScript::Material::Destructor);
		mono_add_internal_call("Frost.Material::SetAlbedoColor_Native", Frost::ScriptInternalCalls::RendererScript::Material::SetAbledoColor);
		mono_add_internal_call("Frost.Material::GetAlbedoColor_Native", Frost::ScriptInternalCalls::RendererScript::Material::GetAbledoColor);
		mono_add_internal_call("Frost.Material::SetMetalness_Native", Frost::ScriptInternalCalls::RendererScript::Material::SetMetalness);
		mono_add_internal_call("Frost.Material::GetMetalness_Native", Frost::ScriptInternalCalls::RendererScript::Material::GetMetalness);
		mono_add_internal_call("Frost.Material::SetRoughness_Native", Frost::ScriptInternalCalls::RendererScript::Material::SetRoughness);
		mono_add_internal_call("Frost.Material::GetRoughness_Native", Frost::ScriptInternalCalls::RendererScript::Material::GetRoughness);
		mono_add_internal_call("Frost.Material::SetEmission_Native", Frost::ScriptInternalCalls::RendererScript::Material::SetEmission);
		mono_add_internal_call("Frost.Material::GetEmission_Native", Frost::ScriptInternalCalls::RendererScript::Material::GetEmission);
		mono_add_internal_call("Frost.Material::GetTextureByString_Native", Frost::ScriptInternalCalls::RendererScript::Material::GetTextureByString);
		mono_add_internal_call("Frost.Material::SetTextureByString_Native", Frost::ScriptInternalCalls::RendererScript::Material::SetTextureByString);

		// Animation Component class native methods
		mono_add_internal_call("Frost.AnimationComponent::GetAnimations_Native", Frost::ScriptInternalCalls::Components::Animation::GetAnimations);
		mono_add_internal_call("Frost.AnimationComponent::SetActiveAnimation_Native", Frost::ScriptInternalCalls::Components::Animation::SetActiveAnimation);

		// Mesh Component class native methods
		mono_add_internal_call("Frost.MeshComponent::GetMesh_Native", Frost::ScriptInternalCalls::Components::Mesh::GetMeshPtr);
		mono_add_internal_call("Frost.MeshComponent::SetMesh_Native", Frost::ScriptInternalCalls::Components::Mesh::SetMesh);
		mono_add_internal_call("Frost.MeshComponent::HasMaterial_Native", Frost::ScriptInternalCalls::Components::Mesh::HasMaterial);
		mono_add_internal_call("Frost.MeshComponent::GetMaterial_Native", Frost::ScriptInternalCalls::Components::Mesh::GetMaterial);

		// Mesh class native methods
		mono_add_internal_call("Frost.Mesh::Constructor_Native", Frost::ScriptInternalCalls::RendererScript::Mesh::Constructor);
		mono_add_internal_call("Frost.Mesh::Destructor_Native", Frost::ScriptInternalCalls::RendererScript::Mesh::Destructor);
		mono_add_internal_call("Frost.Mesh::GetMaterialByIndex_Native", Frost::ScriptInternalCalls::RendererScript::Mesh::GetMaterialByIndex);
		mono_add_internal_call("Frost.Mesh::GetMaterialCount_Native", Frost::ScriptInternalCalls::RendererScript::Mesh::GetMaterialCount);

		// Script Component class native methods
		mono_add_internal_call("Frost.ScriptComponent::GetInstance_Native", Frost::ScriptInternalCalls::Components::Script::GetInstance);

		// Rigid Body Component class native methods
		mono_add_internal_call("Frost.RigidBodyComponent::GetBodyType_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetBodyType);
		mono_add_internal_call("Frost.RigidBodyComponent::SetTranslation_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetTranslation);
		mono_add_internal_call("Frost.RigidBodyComponent::GetTranslation_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetTranslation);
		mono_add_internal_call("Frost.RigidBodyComponent::SetRotation_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetRotation);
		mono_add_internal_call("Frost.RigidBodyComponent::GetRotation_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetRotation);
		mono_add_internal_call("Frost.RigidBodyComponent::SetMass_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetMass);
		mono_add_internal_call("Frost.RigidBodyComponent::GetMass_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetMass);
		mono_add_internal_call("Frost.RigidBodyComponent::SetLinearVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetLinearVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::GetLinearVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetLinearVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::SetAngularVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetAngularVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::GetAngularVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetAngularVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::SetMaxLinearVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetMaxLinearVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::GetMaxLinearVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetMaxLinearVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::SetMaxAngularVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetMaxAngularVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::GetMaxAngularVelocity_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetMaxAngularVelocity);
		mono_add_internal_call("Frost.RigidBodyComponent::GetKinematicTarget_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetKinematicTarget);
		mono_add_internal_call("Frost.RigidBodyComponent::SetKinematicTarget_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetKinematicTarget);
		mono_add_internal_call("Frost.RigidBodyComponent::AddForce_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::AddForce);
		mono_add_internal_call("Frost.RigidBodyComponent::AddTorque_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::AddTorque);
		mono_add_internal_call("Frost.RigidBodyComponent::Rotate_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::Rotate);
		mono_add_internal_call("Frost.RigidBodyComponent::SetLockFlag_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::SetLockFlag);
		mono_add_internal_call("Frost.RigidBodyComponent::IsLockFlagSet_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::IsLockFlagSet);
		mono_add_internal_call("Frost.RigidBodyComponent::GetLockFlags_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetLockFlags);
		mono_add_internal_call("Frost.RigidBodyComponent::GetLayer_Native", Frost::ScriptInternalCalls::Components::Physics::RigidBody::GetLayer);

		// Input class native methods
		mono_add_internal_call("Frost.Input::IsKeyPressed_Native", Frost::ScriptInternalCalls::Input::IsKeyPressed);
		mono_add_internal_call("Frost.Input::IsMouseButtonPressed_Native", Frost::ScriptInternalCalls::Input::IsMouseButtonPressed);
		mono_add_internal_call("Frost.Input::GetMousePosition_Native", Frost::ScriptInternalCalls::Input::GetMousePosition);
		mono_add_internal_call("Frost.Input::SetCursorMode_Native", Frost::ScriptInternalCalls::Input::SetCursorMode);
		mono_add_internal_call("Frost.Input::GetCursorMode_Native", Frost::ScriptInternalCalls::Input::GetCursorMode);

		// Physics native methods
		mono_add_internal_call("Frost.Physics::Raycast_Native", Frost::ScriptInternalCalls::Components::Physics::Raycast);
		mono_add_internal_call("Frost.Physics::GetGravity_Native", Frost::ScriptInternalCalls::Components::Physics::GetGravity);
		mono_add_internal_call("Frost.Physics::SetGravity_Native", Frost::ScriptInternalCalls::Components::Physics::SetGravity);
		mono_add_internal_call("Frost.Physics::OverlapBox_Native", Frost::ScriptInternalCalls::Components::Physics::OverlapBox);
		mono_add_internal_call("Frost.Physics::OverlapCapsule_Native", Frost::ScriptInternalCalls::Components::Physics::OverlapCapsule);
		mono_add_internal_call("Frost.Physics::OverlapSphere_Native", Frost::ScriptInternalCalls::Components::Physics::OverlapSphere);

		// Box Collider native methods
		mono_add_internal_call("Frost.BoxColliderComponent::GetSize_Native", Frost::ScriptInternalCalls::Components::Physics::BoxCollider::GetSize);
		mono_add_internal_call("Frost.BoxColliderComponent::SetSize_Native", Frost::ScriptInternalCalls::Components::Physics::BoxCollider::SetSize);
		mono_add_internal_call("Frost.BoxColliderComponent::GetOffset_Native", Frost::ScriptInternalCalls::Components::Physics::BoxCollider::GetOffset);
		mono_add_internal_call("Frost.BoxColliderComponent::SetOffset_Native", Frost::ScriptInternalCalls::Components::Physics::BoxCollider::SetOffset);
		mono_add_internal_call("Frost.BoxColliderComponent::GetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::BoxCollider::GetIsTrigger);
		mono_add_internal_call("Frost.BoxColliderComponent::SetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::BoxCollider::SetIsTrigger);

		// Sphere Collider native methods
		mono_add_internal_call("Frost.SphereColliderComponent::GetRadius_Native", Frost::ScriptInternalCalls::Components::Physics::SphereCollider::GetRadius);
		mono_add_internal_call("Frost.SphereColliderComponent::SetRadius_Native", Frost::ScriptInternalCalls::Components::Physics::SphereCollider::SetRadius);
		mono_add_internal_call("Frost.SphereColliderComponent::SetOffset_Native", Frost::ScriptInternalCalls::Components::Physics::SphereCollider::SetOffset);
		mono_add_internal_call("Frost.SphereColliderComponent::GetOffset_Native", Frost::ScriptInternalCalls::Components::Physics::SphereCollider::GetOffset);
		mono_add_internal_call("Frost.SphereColliderComponent::GetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::SphereCollider::GetIsTrigger);
		mono_add_internal_call("Frost.SphereColliderComponent::SetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::SphereCollider::SetIsTrigger);

		// Capsule Collider native methods
		mono_add_internal_call("Frost.CapsuleColliderComponent::GetRadius_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::GetRadius);
		mono_add_internal_call("Frost.CapsuleColliderComponent::SetRadius_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::SetRadius);
		mono_add_internal_call("Frost.CapsuleColliderComponent::GetHeight_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::GetHeight);
		mono_add_internal_call("Frost.CapsuleColliderComponent::SetHeight_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::SetHeight);
		mono_add_internal_call("Frost.CapsuleColliderComponent::GetOffset_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::GetOffset);
		mono_add_internal_call("Frost.CapsuleColliderComponent::SetOffset_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::SetOffset);
		mono_add_internal_call("Frost.CapsuleColliderComponent::GetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::GetIsTrigger);
		mono_add_internal_call("Frost.CapsuleColliderComponent::SetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::CapsuleCollider::SetIsTrigger);

		// Mesh Collider native methods
		mono_add_internal_call("Frost.MeshColliderComponent::GetColliderMesh_Native", Frost::ScriptInternalCalls::Components::Physics::MeshCollider::GetColliderMesh);
		mono_add_internal_call("Frost.MeshColliderComponent::IsConvex_Native", Frost::ScriptInternalCalls::Components::Physics::MeshCollider::IsConvex);
		mono_add_internal_call("Frost.MeshColliderComponent::SetConvex_Native", Frost::ScriptInternalCalls::Components::Physics::MeshCollider::SetConvex);
		mono_add_internal_call("Frost.MeshColliderComponent::GetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::MeshCollider::GetIsTrigger);
		mono_add_internal_call("Frost.MeshColliderComponent::SetIsTrigger_Native", Frost::ScriptInternalCalls::Components::Physics::MeshCollider::SetIsTrigger);
	}

}