#pragma once

#include "Frost/Asset/AssetTypes.h"

namespace Frost
{
	inline static HashMap<std::string, AssetType> s_AssetExtensionMap =
	{
		// Engine types (Frost)
		{ ".fsc",  AssetType::Scene },
		{ ".fmat",  AssetType::Material },
		{ ".fprefab",  AssetType::Prefab },
		{ ".fpmat",  AssetType::PhysicsMat },
		{ ".fanim", AssetType::AnimationBlueprint },

		// Meshes
		{ ".fbx",  AssetType::MeshAsset },
		{ ".gltf", AssetType::MeshAsset },
		{ ".obj",  AssetType::MeshAsset },

		// Fonts
		{ ".ttf",  AssetType::Font },
		{ ".otf",  AssetType::Font },

		// Textures
		{ ".png",  AssetType::Texture },
		{ ".jpg",  AssetType::Texture },

		{ ".hdr", AssetType::EnvMap },

	};
}