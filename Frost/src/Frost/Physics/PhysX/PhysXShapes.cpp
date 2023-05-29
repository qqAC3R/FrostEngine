#include "frostpch.h"
#include "PhysXShapes.h"

#include "Frost/Math/Math.h"
#include "PhysXInternal.h"
#include "PhysXActor.h"
#include "CookingFactory.h"

namespace Frost::PhysX
{
	// TODO: Add physics material system
	static physx::PxMaterial* CreatePhysicsMaterial()
	{
		return PhysXInternal::GetPhysXHandle().createMaterial(0.6f, 0.6f, 0.6f);
	}

	BoxColliderShape::BoxColliderShape(BoxColliderComponent& component,
		const PhysXActor& actor,
		Entity entity,
		const glm::vec3& offset)
		: ColliderShape(ColliderType::Box), m_Component(component)
	{

		m_Material = CreatePhysicsMaterial();

		glm::vec3 colliderSize = entity.Transform().Scale * m_Component.Size;
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(colliderSize.x / 2.0f, colliderSize.y / 2.0f, colliderSize.z / 2.0f);

		m_Shape = physx::PxRigidActorExt::createExclusiveShape(
			*actor.GetPhysXActor(),
			geometry, 
			*m_Material
		);
		//m_Shape->setGeometry(PhysXUtils::ToPhysXTransform(offset + m_Component.Offset, glm::vec3(0.0f)));

		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset + m_Component.Offset, glm::vec3(0.0f)));
	}

	BoxColliderShape::~BoxColliderShape()
	{
		m_Material->release();
		//m_Shape->release();
	}

	void BoxColliderShape::SetOffset(const glm::vec3& offset)
	{
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset, glm::vec3(0.0f)));
		m_Component.Offset = offset;
	}

	void BoxColliderShape::SetTrigger(bool isTrigger)
	{
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		m_Component.IsTrigger = isTrigger;
	}

	void BoxColliderShape::SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType)
	{
		physx::PxFilterData filterData;
		filterData.word0 = layerInfo.BitValue;
		filterData.word1 = layerInfo.CollidesWith;
		filterData.word2 = (uint32_t)collisionType;

		m_Shape->setSimulationFilterData(filterData);
	}

	void BoxColliderShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*m_Shape);
	}

	void BoxColliderShape::SetSize(const glm::vec3& transformScale, const glm::vec3& size)
	{
		glm::vec3 colliderSize = transformScale * size;
		physx::PxBoxGeometry geometry = physx::PxBoxGeometry(colliderSize.x / 2.0f, colliderSize.y / 2.0f, colliderSize.z / 2.0f);

		m_Shape->setGeometry(geometry);

		m_Component.Size = size;
	}



	SphereColliderShape::SphereColliderShape(SphereColliderComponent& component,
		const PhysXActor& actor,
		Entity entity,
		const glm::vec3& offset)
		: ColliderShape(ColliderType::Sphere), m_Component(component)
	{
		m_Material = CreatePhysicsMaterial();

		auto& actorScale = entity.Transform().Scale;
		float largestComponent = glm::max(actorScale.x, glm::max(actorScale.y, actorScale.z));

		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(largestComponent * m_Component.Radius);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset + m_Component.Offset, glm::vec3(0.0f)));
	}

	SphereColliderShape::~SphereColliderShape()
	{
		m_Material->release();
		////m_Shape->release();
	}

	void SphereColliderShape::SetOffset(const glm::vec3& offset)
	{
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset, glm::vec3(0.0f)));
		m_Component.Offset = offset;
	}

	void SphereColliderShape::SetTrigger(bool isTrigger)
	{
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		m_Component.IsTrigger = isTrigger;
	}

	void SphereColliderShape::SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType)
	{
		physx::PxFilterData filterData;
		filterData.word0 = layerInfo.BitValue;
		filterData.word1 = layerInfo.CollidesWith;
		filterData.word2 = (uint32_t)collisionType;

		m_Shape->setSimulationFilterData(filterData);
	}

	void SphereColliderShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*m_Shape);
	}

	void SphereColliderShape::SetRadius(const glm::vec3& transformScale, float radius)
	{
		float largestComponent = glm::max(transformScale.x, glm::max(transformScale.y, transformScale.z));
		physx::PxSphereGeometry geometry = physx::PxSphereGeometry(largestComponent * radius);

		m_Shape->setGeometry(geometry);

		m_Component.Radius = radius;
	}




	CapsuleColliderShape::CapsuleColliderShape(CapsuleColliderComponent& component,
		const PhysXActor& actor,
		Entity entity, 
		const glm::vec3& offset)
		: ColliderShape(ColliderType::Capsule), m_Component(component)
	{
		m_Material = CreatePhysicsMaterial();

		auto& actorScale = entity.Transform().Scale;
		float radiusScale = glm::max(actorScale.x, actorScale.z);

		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(m_Component.Radius * radiusScale, (m_Component.Height / 2.0f) * actorScale.y);
		m_Shape = physx::PxRigidActorExt::createExclusiveShape(*actor.GetPhysXActor(), geometry, *m_Material);
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset + m_Component.Offset, glm::vec3(0.0f, 0.0f, physx::PxHalfPi)));
	}

	CapsuleColliderShape::~CapsuleColliderShape()
	{
		m_Material->release();
		//m_Shape->release();
	}

	void CapsuleColliderShape::SetOffset(const glm::vec3& offset)
	{
		m_Shape->setLocalPose(PhysXUtils::ToPhysXTransform(offset, glm::vec3(0.0f)));
		m_Component.Offset = offset;
	}

	void CapsuleColliderShape::SetTrigger(bool isTrigger)
	{
		m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
		m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		m_Component.IsTrigger = isTrigger;
	}

	void CapsuleColliderShape::SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType)
	{
		physx::PxFilterData filterData;
		filterData.word0 = layerInfo.BitValue;
		filterData.word1 = layerInfo.CollidesWith;
		filterData.word2 = (uint32_t)collisionType;

		m_Shape->setSimulationFilterData(filterData);
	}

	void CapsuleColliderShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*m_Shape);
	}

	void CapsuleColliderShape::SetRadius(const glm::vec3& transformScale, float radius)
	{
		float radiusScale = glm::max(transformScale.x, transformScale.z);
		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius * radiusScale, (m_Component.Height / 2.0f) * transformScale.y);

		m_Shape->setGeometry(geometry);

		m_Component.Radius = radius;
	}

	void CapsuleColliderShape::SetHeight(const glm::vec3& transformScale, float height)
	{
		float radiusScale = glm::max(transformScale.x, transformScale.z);
		physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(m_Component.Radius * radiusScale, (height / 2.0f) * transformScale.y);

		m_Shape->setGeometry(geometry);

		m_Component.Height = height;
	}





	ConvexMeshShape::ConvexMeshShape(MeshColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset)
		: ColliderShape(ColliderType::ConvexMesh), m_Component(component)
	{
		FROST_ASSERT_INTERNAL(component.IsConvex);

		m_Material = CreatePhysicsMaterial();


		Vector<MeshColliderData> cookedData;
		cookedData.reserve(component.CollisionMesh->GetSubMeshes().size());
		CookingResult result = CookingFactory::CookMesh(component, false, cookedData);
		FROST_ASSERT_INTERNAL(cookedData.size() > 0);

		if (result != CookingResult::Success)
			return;

		for (auto& colliderData : cookedData)
		{
			glm::vec3 submeshTranslation, submeshRotation, submeshScale;

			Math::DecomposeTransform(colliderData.Transform, submeshTranslation, submeshRotation, submeshScale);

			physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);
			physx::PxConvexMesh* convexMesh = PhysXInternal::GetPhysXHandle().createConvexMesh(input);
			physx::PxConvexMeshGeometry convexGeometry = physx::PxConvexMeshGeometry(
				convexMesh,
				physx::PxMeshScale(PhysXUtils::ToPhysXVector(submeshScale * entity.Transform().Scale))
			);
			convexGeometry.meshFlags = physx::PxConvexMeshGeometryFlag::eTIGHT_BOUNDS;

			physx::PxShape* shape = PhysXInternal::GetPhysXHandle().createShape(convexGeometry, *m_Material, true);
			shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
			shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
			shape->setLocalPose(PhysXUtils::ToPhysXTransform(colliderData.Transform));

			actor.GetPhysXActor()->attachShape(*shape);

			m_Shapes.push_back(shape);

			shape->release();
			convexMesh->release();

			delete[] colliderData.Data;
		}

		cookedData.clear();
	}

	ConvexMeshShape::~ConvexMeshShape()
	{
		m_Material->release();
		//m_Shape->release();
	}

	void ConvexMeshShape::SetTrigger(bool isTrigger)
	{
		for (auto shape : m_Shapes)
		{
			shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
			shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		}

		m_Component.IsTrigger = isTrigger;
	}

	void ConvexMeshShape::SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType)
	{
		physx::PxFilterData filterData;
		filterData.word0 = layerInfo.BitValue;
		filterData.word1 = layerInfo.CollidesWith;
		filterData.word2 = (uint32_t)collisionType;

		for (auto shape : m_Shapes)
			shape->setSimulationFilterData(filterData);
	}

	void ConvexMeshShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		for (auto shape : m_Shapes)
			actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*shape);

		m_Shapes.clear();
	}




	TriangleMeshShape::TriangleMeshShape(MeshColliderComponent& component, const PhysXActor& actor, Entity entity, const glm::vec3& offset)
		: ColliderShape(ColliderType::TriangleMesh), m_Component(component)
	{
		FROST_ASSERT_INTERNAL(!component.IsConvex);

		m_Material = CreatePhysicsMaterial();


		Vector<MeshColliderData> cookedData;
		cookedData.reserve(component.CollisionMesh->GetSubMeshes().size());
		CookingResult result = CookingFactory::CookMesh(component, false, cookedData);
		FROST_ASSERT_INTERNAL(cookedData.size() > 0);

		if (result != CookingResult::Success)
			return;

		for (auto& colliderData : cookedData)
		{
			glm::vec3 submeshTranslation, submeshRotation, submeshScale;

			Math::DecomposeTransform(colliderData.Transform, submeshTranslation, submeshRotation, submeshScale);

			physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);
			physx::PxTriangleMesh* triangleMesh = PhysXInternal::GetPhysXHandle().createTriangleMesh(input);
			physx::PxTriangleMeshGeometry triangleGeometry = physx::PxTriangleMeshGeometry(
				triangleMesh,
				physx::PxMeshScale(PhysXUtils::ToPhysXVector(submeshScale * entity.Transform().Scale))
			);

			physx::PxShape* shape = PhysXInternal::GetPhysXHandle().createShape(triangleGeometry, *m_Material, true);
			shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !m_Component.IsTrigger);
			shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, m_Component.IsTrigger);
			shape->setLocalPose(PhysXUtils::ToPhysXTransform(colliderData.Transform));

			actor.GetPhysXActor()->attachShape(*shape);

			m_Shapes.push_back(shape);

			shape->release();
			triangleMesh->release();

			delete[] colliderData.Data;
		}

		cookedData.clear();
	}

	TriangleMeshShape::~TriangleMeshShape()
	{
		m_Material->release();
		//m_Shape->release();
	}

	void TriangleMeshShape::SetTrigger(bool isTrigger)
	{
		for (auto shape : m_Shapes)
		{
			shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !isTrigger);
			shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, isTrigger);
		}

		m_Component.IsTrigger = isTrigger;
	}

	void TriangleMeshShape::SetFilterData(const PhysicsLayer& layerInfo, CollisionDetectionType collisionType)
	{
		physx::PxFilterData filterData;
		filterData.word0 = layerInfo.BitValue;
		filterData.word1 = layerInfo.CollidesWith;
		filterData.word2 = (uint32_t)collisionType;

		for (auto shape : m_Shapes)
			shape->setSimulationFilterData(filterData);
	}

	void TriangleMeshShape::DetachFromActor(Ref<PhysicsActor> actor)
	{
		for (auto shape : m_Shapes)
			actor.As<PhysXActor>()->GetPhysXActor()->detachShape(*shape);

		m_Shapes.clear();
	}
}