#pragma once

#include "Frost/Physics/PhysicsLayer.h"
#include "Frost/Physics/PhysicsSettings.h"

namespace Frost
{
	// Foward declaration
	class PhysicsActor;

	enum class ColliderType
	{
		Box, Sphere, Capsule, ConvexMesh, TriangleMesh
	};

	class ColliderShape
	{
	protected:
		ColliderShape(ColliderType type)
			: m_Type(type) {}

	public:
		virtual ~ColliderShape() {}

		//void SetMaterial(material);

		virtual const glm::vec3& GetOffset() const = 0;
		virtual void SetOffset(const glm::vec3& offset) = 0;

		virtual bool IsTrigger() const = 0;
		virtual void SetTrigger(bool isTrigger) = 0;

		virtual void SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType) = 0;

		virtual void DetachFromActor(Ref<PhysicsActor> actor) = 0;

		//bool IsValid() const { return m_Material != nullptr; }

	protected:
		ColliderType m_Type;
	};


}