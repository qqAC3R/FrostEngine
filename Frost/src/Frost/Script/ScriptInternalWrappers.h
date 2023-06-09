#pragma once

#include "Frost/EntitySystem/Components.h"

extern "C" {
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
}

namespace Frost
{
	// TODO: Change those naming schemes, it's so fucking horrible to look at them
	struct Texture2DPointer
	{
		Texture2DPointer(Ref<Texture2D> tex)
			: Texture(tex) {}

		~Texture2DPointer() {}

		Ref<Texture2D> Texture;
	};

	// TODO: Change those naming schemes, it's so fucking horrible to look at them
	struct MaterialMeshPointer
	{
		MaterialMeshPointer(Ref<MaterialAsset> material)
			: InternalMaterial(material) {}

		~MaterialMeshPointer() {}

		Ref<MaterialAsset> InternalMaterial;
	};

	// TODO: Change those naming schemes, it's so fucking horrible to look at them
	struct MeshPointer
	{
		MeshPointer(Ref<Frost::Mesh> mesh)
			: Mesh(mesh) {}

		~MeshPointer() {}

		Ref<Frost::Mesh> Mesh;
	};

	// Forward declaration
	enum class ForceMode : uint8_t;
	enum class ActorLockFlag;
	enum class CursorMode;

	struct RaycastHit;

	namespace Key { enum KeyCode; }
	namespace Mouse { enum MouseCode; }
	
}

namespace Frost { namespace ScriptInternalCalls
{
	// Responsible for internal entity calls
	namespace EntityScript
	{
		// Entity
		uint64_t GetParent(uint64_t entityID);
		void SetParent(uint64_t entityID, uint64_t parentID);
		MonoArray* GetChildren(uint64_t entityID);

		void CreateComponent(uint64_t entityID, void* type);
		bool HasComponent(uint64_t entityID, void* type);
		uint64_t FindEntityByTag(MonoString* tag);

		uint64_t CreateEntity();
		void DestroyEntity(uint64_t entityID);
	}

	namespace Input
	{
		bool IsKeyPressed(Key::KeyCode keycode);
		bool IsMouseButtonPressed(Mouse::MouseCode button);
		void GetMousePosition(glm::vec2* position);
		void SetCursorMode(CursorMode mode);
		CursorMode GetCursorMode();
	}

	namespace Log
	{
		enum class LogLevel : int32_t
		{
			Trace = BIT(0),
			Debug = BIT(1),
			Info = BIT(2),
			Warn = BIT(3),
			Error = BIT(4),
			Critical = BIT(5)
		};

		// TODO: Currently, log messages are displayed in console,
		// however they should be displayed in the editor, as a separate section
		void LogMessage(LogLevel level, MonoString* message);
	}


	// Responsible for internal component calls
	namespace Components
	{
		namespace Tag
		{
			MonoString* GetTag(uint64_t entityID);
			void SetTag(uint64_t entityID, MonoString* tag);
		}

		namespace Transform
		{
			void SetTransform(uint64_t entityID, TransformComponent* inTransform);
			void GetTransform(uint64_t entityID, TransformComponent* outTransform);

			void SetTranslation(uint64_t entityID, glm::vec3* inTranslation);
			void GetTranslation(uint64_t entityID, glm::vec3* outTranslation);

			void SetRotation(uint64_t entityID, glm::vec3* inRotation);
			void GetRotation(uint64_t entityID, glm::vec3* outRotation);

			void SetScale(uint64_t entityID, glm::vec3* inScale);
			void GetScale(uint64_t entityID, glm::vec3* outScale);
			
			void GetWorldSpaceTransform(uint64_t entityID, TransformComponent* outTransform);;
		}

		namespace Camera
		{
			void SetFOV(uint64_t entityID, float fov);
			float GetFOV(uint64_t entityID);

			void SetNearClip(uint64_t entityID, float nearClip);
			float GetNearClip(uint64_t entityID);

			void SetFarClip(uint64_t entityID, float farClip);
			float GetFarClip(uint64_t entityID);
		}

		namespace Physics
		{
			struct RaycastData
			{
				glm::vec3 Origin;
				glm::vec3 Direction;
				float MaxDistance;
				MonoArray* RequiredComponentTypes;
			};

			bool Raycast(RaycastData* inData, RaycastHit* hit);
			void GetGravity(glm::vec3* outGravity);
			void SetGravity(glm::vec3* inGravity);
			MonoArray* OverlapBox(glm::vec3* origin, glm::vec3* halfSize);
			MonoArray* OverlapCapsule(glm::vec3* origin, float radius, float halfHeight);
			MonoArray* OverlapSphere(glm::vec3* origin, float radius);

			namespace RigidBody
			{
				Frost::RigidBodyComponent::Type GetBodyType(uint64_t entityID);
				void SetTranslation(uint64_t entityID, glm::vec3* inTranslation);
				void GetTranslation(uint64_t entityID, glm::vec3* outTranslation);

				void SetRotation(uint64_t entityID, glm::vec3* inRotation);
				void GetRotation(uint64_t entityID, glm::vec3* outRotation);

				void SetMass(uint64_t entityID, float mass);
				float GetMass(uint64_t entityID);

				void SetLinearVelocity(uint64_t entityID, glm::vec3* velocity);
				void GetLinearVelocity(uint64_t entityID, glm::vec3* velocity);
				void SetAngularVelocity(uint64_t entityID, glm::vec3* velocity);
				void GetAngularVelocity(uint64_t entityID, glm::vec3* velocity);

				void SetMaxLinearVelocity(uint64_t entityID, float maxVelocity);
				float GetMaxLinearVelocity(uint64_t entityID);
				void SetMaxAngularVelocity(uint64_t entityID, float maxVelocity);
				float GetMaxAngularVelocity(uint64_t entityID);

				void GetKinematicTarget(uint64_t entityID, glm::vec3* outTargetPosition, glm::vec3* outTargetRotation);
				void SetKinematicTarget(uint64_t entityID, glm::vec3* inTargetPosition, glm::vec3* inTargetRotation);

				void AddForce(uint64_t entityID, glm::vec3* force, ForceMode forceMode);
				void AddTorque(uint64_t entityID, glm::vec3* torque, ForceMode forceMode);

				void Rotate(uint64_t entityID, glm::vec3* rotation);

				void SetLockFlag(uint64_t entityID, ActorLockFlag flag, bool value);
				bool IsLockFlagSet(uint64_t entityID, ActorLockFlag flag);
				uint32_t GetLockFlags(uint64_t entityID, ActorLockFlag flag, bool value);

				uint32_t GetLayer(uint64_t entityID);
			}

			namespace BoxCollider
			{
				void SetSize(uint64_t entityID, glm::vec3* inSize);
				void GetSize(uint64_t entityID, glm::vec3* outSize);

				void SetOffset(uint64_t entityID, glm::vec3* inOffset);
				void GetOffset(uint64_t entityID, glm::vec3* outOffset);

				bool GetIsTrigger(uint64_t entityID);
				void SetIsTrigger(uint64_t entityID, bool isTrigger);
			}


			namespace SphereCollider
			{
				float GetRadius(uint64_t entityID);
				void SetRadius(uint64_t entityID, float radius);

				void SetOffset(uint64_t entityID, glm::vec3* inOffset);
				void GetOffset(uint64_t entityID, glm::vec3* outOffset);

				bool GetIsTrigger(uint64_t entityID);
				void SetIsTrigger(uint64_t entityID, bool isTrigger);
			}

			namespace CapsuleCollider
			{
				void SetRadius(uint64_t entityID, float radius);
				float GetRadius(uint64_t entityID);

				void SetHeight(uint64_t entityID, float height);
				float GetHeight(uint64_t entityID);

				void SetOffset(uint64_t entityID, glm::vec3* inOffset);
				void GetOffset(uint64_t entityID, glm::vec3* outOffset);

				bool GetIsTrigger(uint64_t entityID);
				void SetIsTrigger(uint64_t entityID, bool isTrigger);
			}

			namespace MeshCollider
			{
				void* GetColliderMesh(uint64_t entityID);

				bool IsConvex(uint64_t entityID);
				void SetConvex(uint64_t entityID, bool isConvex);

				bool GetIsTrigger(uint64_t entityID);
				void SetIsTrigger(uint64_t entityID, bool isTrigger);
			}

		}

		namespace Mesh
		{
			void* GetMeshPtr(uint64_t entityID);
			void SetMesh(uint64_t entityID, MeshPointer* meshPtr);

			bool HasMaterial(uint64_t entityID, int index);
			void* GetMaterial(uint64_t entityID, int index);
		}

		namespace Animation
		{
			MonoArray* GetAnimations(uint64_t entityID);
			void SetActiveAnimation(uint64_t entityID, MonoString* animationName);
		}

		namespace Script
		{
			ScriptComponent GetInstance(uint64_t entityID);
		}
	}

	// Responsible for internal renderer calls
	namespace RendererScript
	{
		namespace Mesh
		{
			void* Constructor(MonoString* filepath);
			void Destructor(MeshPointer* _this);
			uint32_t GetMaterialCount(MeshPointer* _this);
			void* GetMaterialByIndex(MeshPointer* _this, int index);
		}

		namespace Material
		{
			void Destructor(MaterialMeshPointer* _this);
			void GetAbledoColor(MaterialMeshPointer* material, glm::vec3* outAlbedo);
			void SetAbledoColor(MaterialMeshPointer* material, glm::vec3* inAlbedo);
			void GetMetalness(MaterialMeshPointer* material, float outMetalness);
			void SetMetalness(MaterialMeshPointer* material, float inMetalness);
			void GetRoughness(MaterialMeshPointer* material, float outRoughness);
			void SetRoughness(MaterialMeshPointer* material, float inRoughness);
			void GetEmission(MaterialMeshPointer* material, float outEmission);
			void SetEmission(MaterialMeshPointer* material, float inEmission);

			void* GetTextureByString(MaterialMeshPointer* material, MonoString* textureType);
			void SetTextureByString(MaterialMeshPointer* material, MonoString* textureType, Texture2DPointer* inTexturePtr);
		}

		namespace Texture
		{
			void* Texture2DConstructor(uint32_t width, uint32_t height);
			void* Texture2DWithFilepathConstructor(MonoString* filepath);

			void Texture2DDestructor(Texture2DPointer* _this);

			void SetDataTexture2D(Texture2DPointer* _this, MonoArray* inData, int32_t count);
		}
	}
} }