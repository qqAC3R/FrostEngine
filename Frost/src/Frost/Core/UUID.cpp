#include "frostpch.h"
#include "UUID.h"

namespace Frost
{
	static std::random_device s_RandomDevice;
	static std::mt19937_64 engine(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

	UUID::UUID()
		: m_UUID(s_UniformDistribution(engine))
	{
	}

	UUID::UUID(uint64_t id)
		: m_UUID(id)
	{

	}

	UUID::UUID(const UUID& uuid)
		: m_UUID(uuid.m_UUID)
	{
	}
}