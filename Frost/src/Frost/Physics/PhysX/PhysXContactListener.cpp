#include "frostpch.h"
#include "PhysXContactListener.h"

namespace Frost
{
	void PhysXContactListener::onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count)
	{
		PX_UNUSED(constraints);
		PX_UNUSED(count);
	}

	void PhysXContactListener::onWake(physx::PxActor** actors, physx::PxU32 count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			physx::PxActor& physxActor = *actors[i];
			//Ref<PhysicsActor> actor = (PhysicsActor*)physxActor.userData;
			//HZ_CORE_INFO("PhysX Actor waking up: ID: {0}, Name: {1}", actor->GetEntity().GetUUID(), actor->GetEntity().GetComponent<TagComponent>().Tag);
		}
	}

	void PhysXContactListener::onSleep(physx::PxActor** actors, physx::PxU32 count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			physx::PxActor& physxActor = *actors[i];
			//Ref<PhysicsActor> actor = (PhysicsActor*)physxActor.userData;
			//HZ_CORE_INFO("PhysX Actor going to sleep: ID: {0}, Name: {1}", actor->GetEntity().GetUUID(), actor->GetEntity().GetComponent<TagComponent>().Tag);
		}
	}

	void PhysXContactListener::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
	{
#if 0
		if (!ScriptEngine::GetCurrentSceneContext()->IsPlaying())
			return;

		auto removedActorA = pairHeader.flags & physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_0;
		auto removedActorB = pairHeader.flags & physx::PxContactPairHeaderFlag::eREMOVED_ACTOR_1;

		if (removedActorA || removedActorB)
			return;

		Ref<PhysicsActor> actorA = (PhysicsActor*)pairHeader.actors[0]->userData;
		Ref<PhysicsActor> actorB = (PhysicsActor*)pairHeader.actors[1]->userData;

		if (!actorA || !actorB)
			return;

		bool actorAScriptModuleValid = ScriptEngine::IsEntityModuleValid(actorA->GetEntity());
		bool actorBScriptModuleValid = ScriptEngine::IsEntityModuleValid(actorB->GetEntity());

		if (pairs->flags == physx::PxContactPairFlag::eACTOR_PAIR_HAS_FIRST_TOUCH)
		{
			if (actorAScriptModuleValid)
				ScriptEngine::OnCollisionBegin(actorA->GetEntity(), actorB->GetEntity());
			if (actorBScriptModuleValid)
				ScriptEngine::OnCollisionBegin(actorB->GetEntity(), actorA->GetEntity());
		}
		else if (pairs->flags == physx::PxContactPairFlag::eACTOR_PAIR_LOST_TOUCH)
		{
			if (actorAScriptModuleValid)
				ScriptEngine::OnCollisionEnd(actorA->GetEntity(), actorB->GetEntity());
			if (actorBScriptModuleValid)
				ScriptEngine::OnCollisionEnd(actorB->GetEntity(), actorA->GetEntity());
		}
#endif
	}

	void PhysXContactListener::onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
	{
#if 0
		if (!ScriptEngine::GetCurrentSceneContext()->IsPlaying())
			return;

		for (uint32_t i = 0; i < count; i++)
		{
			if (pairs[i].flags & (physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;

			Ref<PhysicsActor> triggerActor = (PhysicsActor*)pairs[i].triggerActor->userData;
			Ref<PhysicsActor> otherActor = (PhysicsActor*)pairs[i].otherActor->userData;

			if (!triggerActor || !otherActor)
				continue;

			bool actorAScriptModuleValid = ScriptEngine::IsEntityModuleValid(triggerActor->GetEntity());
			bool actorBScriptModuleValid = ScriptEngine::IsEntityModuleValid(otherActor->GetEntity());

			if (pairs[i].status == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
			{
				if (actorAScriptModuleValid)
					ScriptEngine::OnTriggerBegin(triggerActor->GetEntity(), otherActor->GetEntity());
				if (actorBScriptModuleValid)
					ScriptEngine::OnTriggerBegin(otherActor->GetEntity(), triggerActor->GetEntity());
			}
			else if (pairs[i].status == physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
			{
				if (actorAScriptModuleValid)
					ScriptEngine::OnTriggerEnd(triggerActor->GetEntity(), otherActor->GetEntity());
				if (actorBScriptModuleValid)
					ScriptEngine::OnTriggerEnd(otherActor->GetEntity(), triggerActor->GetEntity());
			}
		}
#endif
	}

	void PhysXContactListener::onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count)
	{
		PX_UNUSED(bodyBuffer);
		PX_UNUSED(poseBuffer);
		PX_UNUSED(count);
	}

}