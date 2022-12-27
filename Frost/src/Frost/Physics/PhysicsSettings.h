#pragma once

#include <glm/glm.hpp>

namespace Frost
{
	enum class BroadphaseType
	{
		SweepAndPrune,
		MultiBoxPrune,
		AutomaticBoxPrune
	};

	enum class FrictionType
	{
		Patch,
		OneDirectional,
		TwoDirectional
	};

	enum class ForceMode : uint8_t
	{
		Force = 0,
		Impulse,
		VelocityChange,
		Acceleration
	};

	enum class ActorLockFlag
	{
		TranslationX = BIT(0),
		TranslationY = BIT(1),
		TranslationZ = BIT(2),
		Translation = TranslationX | TranslationY | TranslationZ,

		RotationX = BIT(3),
		RotationY = BIT(4),
		RotationZ = BIT(5),
		Rotation = RotationX | RotationY | RotationZ
	};

	enum class DebugType
	{
		DebugToFile = 0,
		LiveDebug
	};

	struct PhysicsSettings
	{
		float FixedTimestep = 0.02;
		glm::vec3 Gravity = { 0.0f, -9.81f, 0.0f };
		BroadphaseType BroadphaseAlgorithm = BroadphaseType::AutomaticBoxPrune;
		glm::vec3 WorldBoundsMin = glm::vec3(-1.0f);
		glm::vec3 WorldBoundsMax = glm::vec3(1.0f);
		uint32_t WorldBoundsSubdivisions = 2;
		FrictionType FrictionModel = FrictionType::Patch;
		uint32_t SolverIterations = 8;
		uint32_t SolverVelocityIterations = 2;

#ifdef FROST_DEBUG
		bool DebugOnPlay = true;
		DebugType DebugType = DebugType::LiveDebug;
#endif
	};

}
