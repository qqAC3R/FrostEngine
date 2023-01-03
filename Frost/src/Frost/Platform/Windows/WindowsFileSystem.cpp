#include "frostpch.h"
#include "Frost/Utils/FileSystem.h"

#include <filesystem>

namespace Frost
{
	bool FileSystem::WriteBytes(const std::filesystem::path& filepath, const Buffer& buffer)
	{
		std::ofstream stream(filepath, std::ios::binary | std::ios::trunc);

		if (!stream)
		{
			stream.close();
			return false;
		}

		stream.write((char*)buffer.Data, buffer.Size);
		stream.close();

		return true;
	}

	Buffer FileSystem::ReadBytes(const std::filesystem::path& filepath)
	{
		Buffer buffer;

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		FROST_ASSERT_INTERNAL(stream);

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint32_t size = end - stream.tellg();
		FROST_ASSERT_INTERNAL(size != 0);

		buffer.Allocate(size);
		stream.read((char*)buffer.Data, buffer.Size);

		return buffer;
	}
}