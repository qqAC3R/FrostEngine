#include "frostpch.h"
#include "PhysXDebugger.h"

#include "Frost/Physics/PhysX/PhysXInternal.h"

namespace Frost
{
	struct PhysXData
	{
		physx::PxPvd* Debugger;
		physx::PxPvdTransport* Transport;
	};

	static PhysXData* s_Data = nullptr;


	void PhysXDebugger::Initialize()
	{
		s_Data = new PhysXData();

		s_Data->Debugger = PxCreatePvd(PhysXInternal::GetFoundation());
		FROST_ASSERT(s_Data->Debugger, "PxCreatePvd failed");
	}

	void PhysXDebugger::StartDebugging(const std::string& filepath, bool networkDebugging /*= false*/)
	{
		StopDebugging();

		if (!networkDebugging)
		{
			s_Data->Transport = physx::PxDefaultPvdFileTransportCreate((filepath + ".pxd2").c_str());
			s_Data->Debugger->connect(*s_Data->Transport, physx::PxPvdInstrumentationFlag::eALL);
		}
		else
		{
			s_Data->Transport = physx::PxDefaultPvdSocketTransportCreate("localhost", 5425, 1000);
			s_Data->Debugger->connect(*s_Data->Transport, physx::PxPvdInstrumentationFlag::eALL);
		}
	}


	void PhysXDebugger::Shutdown()
	{
		s_Data->Debugger->release();
		delete s_Data;
		s_Data = nullptr;
	}

	bool PhysXDebugger::IsDebugging()
	{
		return s_Data->Debugger->isConnected();
	}

	void PhysXDebugger::StopDebugging()
	{
		if (!s_Data->Debugger->isConnected())
			return;

		s_Data->Debugger->disconnect();
		s_Data->Transport->release();
	}

	physx::PxPvd* PhysXDebugger::GetDebugger()
	{
		return s_Data->Debugger;
	}

}