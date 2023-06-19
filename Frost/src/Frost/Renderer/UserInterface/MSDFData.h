#pragma once

#include <msdf-atlas-gen.h>
#include "Frost/Core/Engine.h"

namespace Frost
{
	struct MSDFData
	{
		msdf_atlas::FontGeometry FontGeometry;
		Vector<msdf_atlas::GlyphGeometry> Glyphs;
	};
}