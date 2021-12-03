#pragma once

#include "Memory.h"
#include <memory>

#ifdef _WIN32
	/* Windows x64/x86 */
	#ifdef _WIN64
	/* Windows x64 */
		#define FROST_PLATFORM_WINDOW
	#else
	/* Windows x86 */
		#error "x86 Builds are not suppported!"
	#endif
#endif

#ifdef FROST_DEBUG
	#define FROST_ASSERT(x, ...) { if(!x) { FROST_CORE_ERROR(__VA_ARGS__); __debugbreak(); } }
	#define FROST_ASSERT_MSG(...) { FROST_CORE_ERROR(__VA_ARGS__); __debugbreak(); }
	#define FROST_ASSERT_INTERNAL(x) { if(!x) { __debugbreak(); } }
#else
	#define FROST_ASSERT(x, ...)
	#define FROST_ASSERT_MSG(...)
	#define FROST_ASSERT_INTERNAL(x)
#endif

#define BIT(x) (1 << x)
#define FROST_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

namespace Frost
{
	template <typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return Ref<T>::Create(std::forward<Args>(args)...);
	}

	template<typename T>
	using Vector = std::vector<T>;
	
	template<typename T1, typename T2>
	using HashMap = std::unordered_map<T1, T2>;

	using Byte = uint8_t;
}