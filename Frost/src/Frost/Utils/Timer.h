#pragma once

#include "frostpch.h"

#include <chrono>

namespace Frost
{

	class Timer
	{
	public:
		Timer(const char* name)
			: m_Name(name)
		{
			m_StartTimePoint = std::chrono::high_resolution_clock::now();
		}

		virtual ~Timer()
		{
			auto EndTimePoint = std::chrono::high_resolution_clock::now();

			long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimePoint).time_since_epoch().count();
			long long end = std::chrono::time_point_cast<std::chrono::microseconds>(EndTimePoint).time_since_epoch().count();

			float duration = (end - start) * 0.001f;

			FROST_CORE_TRACE("{0}: {1} ms", m_Name, duration);
		}

	private:
		const char* m_Name;
		std::chrono::time_point<std::chrono::steady_clock> m_StartTimePoint;
	};

}