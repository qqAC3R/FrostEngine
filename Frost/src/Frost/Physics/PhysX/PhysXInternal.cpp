#include "frostpch.h"
#include "PhysXInternal.h"

#include <PhysX/extensions/PxDefaultAllocator.h>
#include <PhysX/extensions/PxDefaultCpuDispatcher.h>

#include "Frost/EntitySystem/Components.h"

#include "PhysXDebugger.h"

namespace Frost
{
	struct PhysXData
	{
		physx::PxFoundation* PhysXFoundation;
		physx::PxDefaultCpuDispatcher* PhysXCPUDispatcher;
		physx::PxPhysics* PhysXSDK;

		physx::PxDefaultAllocator Allocator;
		PhysicsErrorCallback ErrorCallback;
		PhysicsAssertHandler AssertHandler;
	};

	static PhysXData* s_PhysXData;

	void PhysXInternal::Initialize()
	{
		FROST_ASSERT(!s_PhysXData, "Trying to initialize the PhysX SDK multiple times!");

		s_PhysXData = new PhysXData();

		s_PhysXData->PhysXFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_PhysXData->Allocator, s_PhysXData->ErrorCallback);
		FROST_ASSERT(s_PhysXData->PhysXFoundation, "PxCreateFoundation failed.");


		physx::PxTolerancesScale scale = physx::PxTolerancesScale();
		scale.length = 1.0f;
		scale.speed = 100.0f;


		PhysXDebugger::Initialize();


#ifdef FROST_DEBUG
		static bool s_TrackMemoryAllocations = true;
#else
		static bool s_TrackMemoryAllocations = false;
#endif

		s_PhysXData->PhysXSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *s_PhysXData->PhysXFoundation, scale, s_TrackMemoryAllocations, PhysXDebugger::GetDebugger());

		bool extentionsLoaded = PxInitExtensions(*s_PhysXData->PhysXSDK, PhysXDebugger::GetDebugger());
		FROST_ASSERT(extentionsLoaded, "Failed to initialize PhysX Extensions.");

		s_PhysXData->PhysXCPUDispatcher = physx::PxDefaultCpuDispatcherCreate(1);




		// TODO:????
		//CookingFactory::Initialize();

		PxSetAssertHandler(s_PhysXData->AssertHandler);
	}

	physx::PxFoundation& PhysXInternal::GetFoundation() { return *s_PhysXData->PhysXFoundation; }
	physx::PxPhysics& PhysXInternal::GetPhysXHandle() { return *s_PhysXData->PhysXSDK; }
	physx::PxCpuDispatcher* PhysXInternal::GetCPUDispatcher() { return s_PhysXData->PhysXCPUDispatcher; }
	physx::PxDefaultAllocator& PhysXInternal::GetAllocator() { return s_PhysXData->Allocator; }

	void PhysXInternal::Shutdown()
	{
		//CookingFactory::Shutdown();

		PhysXDebugger::Shutdown();

		s_PhysXData->PhysXFoundation->release();
		s_PhysXData->PhysXFoundation = nullptr;

		PxCloseExtensions();

		s_PhysXData->PhysXSDK->release();
		s_PhysXData->PhysXSDK = nullptr;

		delete s_PhysXData;
		s_PhysXData = nullptr;
	}

	physx::PxFilterFlags PhysXInternal::FilterShader(
		physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0,
		physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1,
		physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize
	)
	{
#if 0
		if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
		{
			pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
			return physx::PxFilterFlag::eDEFAULT;
		}

		pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;

		if (filterData0.word2 == (uint32_t)RigidBodyComponent::CollisionDetectionType::Continuous || filterData1.word2 == (uint32_t)RigidBodyComponent::CollisionDetectionType::Continuous)
		{
			pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
			pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
		}

		if ((filterData0.word0 & filterData1.word1) || (filterData1.word0 & filterData0.word1))
		{
			pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
			pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
			return physx::PxFilterFlag::eDEFAULT;
		}

		return physx::PxFilterFlag::eSUPPRESS;
#endif

		pairFlags = physx::PxPairFlag::eSOLVE_CONTACT;
		pairFlags |= physx::PxPairFlag::eDETECT_DISCRETE_CONTACT;
		pairFlags |= physx::PxPairFlag::eDETECT_CCD_CONTACT;
		return physx::PxFilterFlags();

	}

	void PhysicsErrorCallback::reportError(physx::PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		const char* errorMessage = NULL;

		switch (code)
		{
		case physx::PxErrorCode::eNO_ERROR:				errorMessage = "No Error"; break;
		case physx::PxErrorCode::eDEBUG_INFO:			errorMessage = "Info"; break;
		case physx::PxErrorCode::eDEBUG_WARNING:		errorMessage = "Warning"; break;
		case physx::PxErrorCode::eINVALID_PARAMETER:	errorMessage = "Invalid Parameter"; break;
		case physx::PxErrorCode::eINVALID_OPERATION:	errorMessage = "Invalid Operation"; break;
		case physx::PxErrorCode::eOUT_OF_MEMORY:		errorMessage = "Out Of Memory"; break;
		case physx::PxErrorCode::eINTERNAL_ERROR:		errorMessage = "Internal Error"; break;
		case physx::PxErrorCode::eABORT:				errorMessage = "Abort"; break;
		case physx::PxErrorCode::ePERF_WARNING:			errorMessage = "Performance Warning"; break;
		case physx::PxErrorCode::eMASK_ALL:				errorMessage = "Unknown Error"; break;
		}

		switch (code)
		{
		case physx::PxErrorCode::eNO_ERROR:
		case physx::PxErrorCode::eDEBUG_INFO:
			FROST_CORE_INFO("[PhysX]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
			break;
		case physx::PxErrorCode::eDEBUG_WARNING:
		case physx::PxErrorCode::ePERF_WARNING:
			FROST_CORE_WARN("[PhysX]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
			break;
		case physx::PxErrorCode::eINVALID_PARAMETER:
		case physx::PxErrorCode::eINVALID_OPERATION:
		case physx::PxErrorCode::eOUT_OF_MEMORY:
		case physx::PxErrorCode::eINTERNAL_ERROR:
			FROST_CORE_ERROR("[PhysX]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
			break;
		case physx::PxErrorCode::eABORT:
		case physx::PxErrorCode::eMASK_ALL:
			FROST_CORE_ERROR("[PhysX]: {0}: {1} at {2} ({3})", errorMessage, message, file, line);
			FROST_ASSERT_INTERNAL(false);
			break;
		}
	}

	void PhysicsAssertHandler::operator()(const char* exp, const char* file, int line, bool& ignore)
	{
		FROST_ASSERT("[PhysX Error]: {0}:{1} - {2}", file, line, exp);
	}


}