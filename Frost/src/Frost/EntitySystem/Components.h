#pragma once

// Frost libraries
#include "Frost/Core/UUID.h"
#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/RuntimeCamera.h"
#include "Frost/Renderer/UserInterface/Font.h"
#include "Frost/Script/ScriptModuleField.h"
#include "Frost/Physics/PhysicsShapes.h"
#include "Frost/Physics/PhysicsMaterial.h"

// Math
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Frost
{
	struct IDComponent
	{
		IDComponent() = default;
		IDComponent(const UUID& id)
			: ID(id)
		{
		}

		UUID ID;
	};

	struct ParentChildComponent
	{
		ParentChildComponent() = default;
		ParentChildComponent(const UUID& parentID)
			: ParentID(parentID), ChildIDs({}) {}

		UUID ParentID = 0;
		Vector<UUID> ChildIDs;
	};

	struct PrefabComponent
	{
		PrefabComponent() = default;
		PrefabComponent(const PrefabComponent& other) = default;

		UUID PrefabAssetHandle = 0;
	};

	struct TagComponent
	{
		TagComponent() = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		}

		std::string Tag;
	};

	struct TransformComponent
	{
		TransformComponent() = default;
		TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
			: Translation(translation), Rotation(rotation), Scale(scale)
		{
		}

		glm::mat4 GetTransform()
		{
			glm::mat4 rotation =  glm::toMat4(glm::quat(glm::radians(Rotation)));
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
			return transform;
		}

		glm::vec3 Translation = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 Rotation    = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::vec3 Scale       = glm::vec3(1.0f, 1.0f, 1.0f);
	};

	struct MeshComponent
	{
		MeshComponent() = default;
		MeshComponent(const Ref<Mesh>& mesh)
			: Mesh(mesh)
		{
		}

		Ref<Mesh> Mesh = nullptr;

		bool IsMeshValid()
		{
			// Check if the mesh pointer is valid, then check if it loaded
			if (Mesh)
			{
				return Mesh->IsLoaded();
			}
			return false;
		}
	};

	struct AnimationComponent
	{
		AnimationComponent() = default;

		AnimationComponent(const MeshComponent* mesh)
			: Controller(CreateRef<AnimationController>(mesh->Mesh)), MeshComponentPtr(mesh)
		{}

		AnimationComponent(Ref<AnimationController> animationController, const MeshComponent* mesh)
			: Controller(animationController), MeshComponentPtr(mesh)
		{}

		void ResetAnimationBlueprint()
		{
			Controller = CreateRef<AnimationController>(MeshComponentPtr->Mesh);
		}

		const MeshComponent* MeshComponentPtr;
		Ref<AnimationController> Controller;
	};

	struct CameraComponent
	{
		CameraComponent()
			: Camera(Ref<RuntimeCamera>::Create(85.0f, 1.0f, 0.1f, 1000.0f)), Primary(true)
		{}

		Ref<RuntimeCamera> Camera;
		bool Primary = true;
	};

	struct PointLightComponent
	{
		PointLightComponent() = default;
		PointLightComponent(glm::vec3 color, float intensity, float radius, float falloff)
			: Color(color), Intensity(intensity), Radius(radius), Falloff(falloff)
		{
		}

		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 3.0f;
		float Falloff = 0.0f;
	};

	struct DirectionalLightComponent
	{
		DirectionalLightComponent() = default;

		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Size = 1.0f;

		float VolumeDensity = 0.0f;
		float Absorption = 1.0f;
		float Phase = 0.0f;
	};

	struct RectangularLightComponent
	{
		RectangularLightComponent() = default;
		RectangularLightComponent(glm::vec3 radiance, float intensity, float twoSided)
			: Radiance(radiance), Intensity(intensity), TwoSided(twoSided)
		{
		}

		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float Radius = 1.0f;
		bool TwoSided = false;
	};

	struct FogBoxVolumeComponent
	{
		FogBoxVolumeComponent() = default;
		FogBoxVolumeComponent(glm::vec3 mieScattering, float phaseValue, glm::vec3 emission, float absorption)
			: MieScattering(mieScattering), PhaseValue(phaseValue), Emission(emission), Absorption(absorption)
		{
		}

		glm::vec3 MieScattering = { 1.0f, 1.0f, 1.0f };
		float PhaseValue = 0.0f;
		glm::vec3 Emission = { 0.0f, 0.0f, 0.0f };
		float Absorption = 1.0f;
		float Density = 1.0f;
	};

	struct CloudVolumeComponent
	{
		CloudVolumeComponent() = default;
		CloudVolumeComponent(float cloudScale)
			: CloudScale(cloudScale)
		{
		}

		float CloudScale = 1.0f;
		float Density = 1.0f;
		
		glm::vec3 Scattering = { 1.0f, 1.0f, 1.0f };
		float PhaseFunction = 0.75f;

		float DensityOffset = 10.0f;
		float DetailOffset = 0.0f; // Detail noise offset
		float CloudAbsorption = 0.75f;
		float SunAbsorption = 1.25f;
	};

	struct SkyLightComponent
	{
		SkyLightComponent() = default;

		bool IsValid() { return RadianceMap && IrradianceMap && PrefilteredMap; }

		bool IsActive = false;
		std::string Filepath;
		Ref<TextureCubeMap> RadianceMap;
		Ref<TextureCubeMap> IrradianceMap;
		Ref<TextureCubeMap> PrefilteredMap;
	};

	struct RigidBodyComponent
	{
		enum class Type { Static, Dynamic };
		//enum class CollisionDetectionType : uint32_t { Discrete, Continuous, ContinuousSpeculative };

		Type BodyType = Type::Dynamic;
		float Mass = 1.0f;
		float LinearDrag = 0.01f;
		float AngularDrag = 0.05f;
		bool DisableGravity = false;
		bool IsKinematic = false;
		uint32_t Layer = 0;
		CollisionDetectionType CollisionDetection = CollisionDetectionType::Discrete;

		bool LockPositionX = false;
		bool LockPositionY = false;
		bool LockPositionZ = false;
		bool LockRotationX = false;
		bool LockRotationY = false;
		bool LockRotationZ = false;

		RigidBodyComponent() = default;
		RigidBodyComponent(const RigidBodyComponent& other) = default;
	};

	struct BoxColliderComponent
	{
		glm::vec3 Size = { 1.0f, 1.0f, 1.0f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		Ref<PhysicsMaterial> MaterialHandle;

		// Collider handle is used to manipulate the internal collider of the physics engine during runtime in the script engine
		// Currently we don't have any other method of obtaning the collider handle, apart from this one
		Ref<ColliderShape> ColliderHandle;

		// The mesh that will be drawn in the editor to show the collision bounds
		Ref<MeshAsset> DebugMesh;

		BoxColliderComponent()
			: DebugMesh(MeshAsset::GetDefaultMeshes().Cube)
		{
			ResetPhysicsMaterial();
		}
		BoxColliderComponent(const BoxColliderComponent & other) = default;

		void ResetPhysicsMaterial()
		{
			MaterialHandle = Ref<PhysicsMaterial>::Create();
			MaterialHandle->Handle = 0;
		}
	};

	struct SphereColliderComponent
	{
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		Ref<PhysicsMaterial> MaterialHandle;

		// The mesh that will be drawn in the editor to show the collision bounds
		Ref<MeshAsset> DebugMesh;

		// Collider handle is used to manipulate the internal collider of the physics engine during runtime in the script engine
		// Currently we don't have any other method of obtaning the collider handle, apart from this one
		Ref<ColliderShape> ColliderHandle;

		SphereColliderComponent()
			: DebugMesh(MeshAsset::GetDefaultMeshes().Sphere)
		{
			ResetPhysicsMaterial();
		}
		SphereColliderComponent(const SphereColliderComponent& other) = default;

		void ResetPhysicsMaterial()
		{
			MaterialHandle = Ref<PhysicsMaterial>::Create();
			MaterialHandle->Handle = 0;
		}
	};

	struct CapsuleColliderComponent
	{
		float Radius = 0.5f;
		float Height = 1.0f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };
		bool IsTrigger = false;
		Ref<PhysicsMaterial> MaterialHandle;

		// The mesh that will be drawn in the editor to show the collision bounds
		Ref<MeshAsset> DebugMesh;

		// Collider handle is used to manipulate the internal collider of the physics engine during runtime in the script engine
		// Currently we don't have any other method of obtaning the collider handle, apart from this one
		Ref<ColliderShape> ColliderHandle;

		CapsuleColliderComponent()
			: DebugMesh(MeshAsset::GetDefaultMeshes().Capsule)
		{
			ResetPhysicsMaterial();
		}
		CapsuleColliderComponent(const CapsuleColliderComponent& other) = default;

		void ResetPhysicsMaterial()
		{
			MaterialHandle = Ref<PhysicsMaterial>::Create();
			MaterialHandle->Handle = 0;
		}
	};

	struct MeshColliderComponent
	{
		Vector<Ref<MeshAsset>> ProcessedMeshes; // Debug Mesh
		Ref<MeshAsset> CollisionMesh = nullptr; // Mesh
		bool IsConvex = false;
		bool IsTrigger = false;
		bool OverrideMesh = false; // TODO: What to do with this?
		Ref<PhysicsMaterial> MaterialHandle;

		// Collider handle is used to manipulate the internal collider of the physics engine during runtime in the script engine
		// Currently we don't have any other method of obtaning the collider handle, apart from this one
		Ref<ColliderShape> ColliderHandle;

		MeshColliderComponent()
		{
			ResetPhysicsMaterial();
		}
		MeshColliderComponent(const MeshColliderComponent& other) = default;
		
		void ResetMesh()
		{
			CollisionMesh = nullptr;
			ProcessedMeshes.clear();
		}

		void ResetPhysicsMaterial()
		{
			MaterialHandle = Ref<PhysicsMaterial>::Create();
			MaterialHandle->Handle = 0;
		}
	};

	struct ScriptComponent
	{
		std::string ModuleName;
		ScriptModuleFieldMap ModuleFieldMap;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent& other) = default;
		ScriptComponent(const std::string& moduleName)
			: ModuleName(moduleName) {}
	};

	struct TextComponent
	{
		std::string TextString = "Text";

		// Font
		Ref<Font> FontAsset;
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float LineSpacing = 0.0f;
		float Kerning = 0.0f;

		// Layout
		float MaxWidth = 10.0f;

		TextComponent() = default;
		TextComponent(const TextComponent& other) = default;
	};

}