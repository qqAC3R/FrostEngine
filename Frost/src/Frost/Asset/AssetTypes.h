#pragma once

#include "Frost/Core/Engine.h"

namespace Frost
{
	enum class AssetFlag : uint16_t
	{
		None = 0,
		Missing = BIT(0),
		Invalid = BIT(1)
	};

	enum class AssetType : uint16_t
	{
		None = 0,
		Scene = 1,
		MeshAsset = 2,
		Material = 3,
		Prefab = 4,
		PhysicsMat = 5,
		Texture = 6,
		EnvMap = 7, // TODO: Not sure if I would like to include this?
		Font = 8,
		AnimationBlueprint = 9
	};

	namespace Utils
	{
		inline AssetType AssetTypeFromString(const std::string& assetType)
		{
			if (assetType == "None")        return AssetType::None;
			if (assetType == "Prefab")      return AssetType::Prefab;
			if (assetType == "Scene")       return AssetType::Scene;
			if (assetType == "MeshAsset")   return AssetType::MeshAsset;
			if (assetType == "Material")    return AssetType::Material;
			if (assetType == "EnvMap")      return AssetType::EnvMap;
			if (assetType == "Texture")     return AssetType::Texture;
			if (assetType == "PhysicsMat")  return AssetType::PhysicsMat;
			if (assetType == "Font")        return AssetType::Font;
			if (assetType == "AnimBlueprint") return AssetType::AnimationBlueprint;

			FROST_ASSERT_MSG("Unknown Asset Type");
			return AssetType::None;
		}

		inline const char* AssetTypeToString(AssetType assetType)
		{
			switch (assetType)
			{
				case AssetType::None:        return "None";
				case AssetType::Prefab:      return "Prefab";
				case AssetType::Scene:		 return "Scene";
				case AssetType::MeshAsset:   return "MeshAsset";
				case AssetType::Material:    return "Material";
				case AssetType::EnvMap:      return "EnvMap";
				case AssetType::Texture:     return "Texture";
				case AssetType::PhysicsMat:  return "PhysicsMat";
				case AssetType::Font:        return "Font";
				case AssetType::AnimationBlueprint: return "AnimBlueprint";
			}

			FROST_ASSERT_MSG("Unknown Asset Type");
			return "None";
		}
	}


}