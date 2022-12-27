#pragma once

#include <PhysX/PxPhysicsAPI.h>

namespace Frost
{

	class PhysicsErrorCallback : public physx::PxErrorCallback
	{
	public:
		virtual void reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line) override;
	};

	class PhysicsAssertHandler : public physx::PxAssertHandler
	{
		virtual void operator()(const char* exp, const char* file, int line, bool& ignore);
	};

	class PhysXInternal
	{
	public:
		static void Initialize();
		static void Shutdown();

		static physx::PxFoundation& GetFoundation();
		static physx::PxPhysics& GetPhysXHandle();
		static physx::PxCpuDispatcher* GetCPUDispatcher();
		static physx::PxDefaultAllocator& GetAllocator();


		// The custom filter shader to use for collision filtering.
		static physx::PxFilterFlags FilterShader(physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1,
			physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize);
	};

}