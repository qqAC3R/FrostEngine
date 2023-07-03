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

		void CopyFrom(PhysicsMaterial* physicsMaterial)
		{
			StaticFriction = physicsMaterial->StaticFriction;
			DynamicFriction = physicsMaterial->DynamicFriction;
			Bounciness = physicsMaterial->Bounciness;
			m_MaterialName = physicsMaterial->m_MaterialName;
		}

		virtual bool ReloadData(const std::string& filepath) override;

		static AssetType GetStaticType() { return AssetType::PhysicsMat; }
		virtual AssetType GetAssetType() const override { return AssetType::PhysicsMat;; }
	};
}