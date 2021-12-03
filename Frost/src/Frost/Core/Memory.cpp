#include "frostpch.h"

namespace Frost
{
	static std::unordered_set<void*> s_LiveReferences;
	static std::mutex s_LiveReferenceMutex;

	namespace RefUtils {

		void AddToLiveReferences(void* instance)
		{
			std::scoped_lock<std::mutex> lock(s_LiveReferenceMutex);
			FROST_ASSERT_INTERNAL(instance);
			s_LiveReferences.insert(instance);
		}

		void RemoveFromLiveReferences(void* instance)
		{
			std::scoped_lock<std::mutex> lock(s_LiveReferenceMutex);
			FROST_ASSERT_INTERNAL(instance);
			FROST_ASSERT_INTERNAL(bool(s_LiveReferences.find(instance) != s_LiveReferences.end()));
			s_LiveReferences.erase(instance);
		}

		bool IsLive(void* instance)
		{
			FROST_ASSERT_INTERNAL(instance);
			return s_LiveReferences.find(instance) != s_LiveReferences.end();
		}
	}
}