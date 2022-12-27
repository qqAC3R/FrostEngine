#pragma once

namespace Frost
{
	class UUID
	{
	public:
		UUID();
		UUID(uint64_t id);
		UUID(const UUID& uuid);
		
		uint64_t Get() const { return m_UUID; }
		operator uint64_t() { return m_UUID; }
		operator const uint64_t() const { return m_UUID; }
	private:
		uint64_t m_UUID;
	};
}

namespace std {

	template <>
	struct hash<Frost::UUID>
	{
		std::size_t operator()(const Frost::UUID& uuid) const
		{
			return hash<uint64_t>()((uint64_t)uuid);
		}
	};
}