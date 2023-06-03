#pragma once

#include "Frost/Asset/AssetTypes.h"

namespace Frost
{
	inline static HashMap<std::string, AssetType> s_AssetExtensionMap =
	{
		// Engine types (Frost)
		{ ".fsc",  AssetType::Scene },
		{ ".fmat",  AssetType::Material },
		{ ".fprefab",  AssetType::Material },
		{ ".fpm",  AssetType::PhysicsMat },

		// Meshes
		{ ".fbx",  AssetType::MeshAsset },
		{ ".gltf", AssetType::MeshAsset },
		{ ".obj",  AssetType::MeshAsset },

		{ ".hdr", AssetType::EnvMap }
	};
}