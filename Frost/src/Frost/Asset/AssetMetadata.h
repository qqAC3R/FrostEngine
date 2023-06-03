#pragma once

#include "Frost/Asset/Asset.h"

#include <filesystem>

namespace Frost
{
	struct AssetMetadata
	{
		AssetHandle Handle = 0;
		AssetType Type;

		std::filesystem::path FilePath;
		bool IsDataLoaded = false;

		bool IsValid() const { return Handle != 0; }
	};

}