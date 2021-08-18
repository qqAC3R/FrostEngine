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


#define FROST_ASSERT(x, ...) { if(!x) { FROST_CORE_ERROR(__VA_ARGS__); __debugbreak(); } }
//#define FROST_ASSERT(x) { if(!x) { __debugbreak(); } }

#define FROST_VKCHECK(x, ...) { if(x != VK_SUCCESS) { FROST_CORE_ERROR(__VA_ARGS__); __debugbreak(); } }
//#define FROST_VKCHECK(x) { if(x != VK_SUCCESS) { __debugbreak(); } }


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
	
	//template <typename T>
	//using Ref = std::shared_ptr<T>;
	//template<typename T, typename ... Args>
	//constexpr Ref<T> CreateRef(Args&& ... args)
	//{
	//	return std::make_shared<T>(std::forward<Args>(args)...);
	//}

	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return Ref<T>::Create(std::forward<Args>(args)...);
	}

	

	template<typename T>
	using Vector = std::vector<T>;
	
}