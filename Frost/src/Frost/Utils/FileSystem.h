#pragma once

#include "Frost/Core/Buffer.h"

#include <filesystem>

namespace Frost
{
	class FileSystem
	{
	public:
		static bool CreateDirectory(const std::filesystem::path& directory);
		static bool CreateDirectory(const std::string& directory);
		static bool Exists(const std::filesystem::path& filepath);
		static bool Exists(const std::string& filepath);
		static bool DeleteFile(const std::filesystem::path& filepath);
		static bool MoveFile(const std::filesystem::path& filepath, const std::filesystem::path& dest);
		static bool IsDirectory(const std::filesystem::path& filepath);

		static std::filesystem::file_time_type GetLastWriteTime(const std::string& filepath);

		static bool Move(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static bool Rename(const std::filesystem::path& oldFilepath, const std::filesystem::path& newFilepath);
		static bool RenameFilename(const std::filesystem::path& oldFilepath, const std::string& newName);

		static bool WriteBytes(const std::filesystem::path& filepath, const Buffer& buffer);
		static Buffer ReadBytes(const std::filesystem::path& filepath);

		static bool ShowFileInExplorer(const std::filesystem::path& path);
		static bool OpenDirectoryInExplorer(const std::filesystem::path& path);
		static bool OpenExternally(const std::filesystem::path& path);
	};
}