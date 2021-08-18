#pragma once

namespace Frost::Math
{

	template <class integral>
	constexpr integral AlignUp(integral x, size_t a) noexcept
	{
		return integral((x + (integral(a) - 1)) & ~integral(a - 1));
	}


}