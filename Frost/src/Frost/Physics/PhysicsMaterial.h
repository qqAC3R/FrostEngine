#pragma once

#include "Frost/Asset/Asset.h"

namespace Frost
{
	class PhysicsMaterial : public Asset
	{
	public:
		float StaticFriction = 0.6f;
		float DynamicFriction = 0.6f;
		float Bounciness = 0.6f;
		std::string m_MaterialName;

		PhysicsMaterial() = default;
		PhysicsMaterial(float staticFriction, float dynamicFriction, float bounciness)
			: StaticFriction(staticFriction), DynamicFriction(dynamicFriction), Bounciness(bounciness), m_MaterialName("")
		{
		}

		static AssetType GetStaticType() { return AssetType::PhysicsMat; }
		virtual AssetType GetAssetType() const override { return AssetType::PhysicsMat;; }
	};
}