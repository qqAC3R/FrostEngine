#pragma once

#include <filesystem>

namespace Frost
{
	struct ProjectConfig
	{
		std::string Name;

		std::string AssetDirectory;
		std::string AssetRegistryPath;

		std::string ScriptModulePath;
		std::string DefaultNamespace;

		std::string StartScene;

		bool ReloadAssemblyOnPlay;

		//std::string ProjectFileName;
		std::string ProjectDirectory;
	};

	class Project
	{
	public:
		Project();
		~Project();

		const ProjectConfig& GetConfig() const { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }
		static void SetActive(Ref<Project> project);

		static const std::string& GetProjectName()
		{
			FROST_ASSERT_INTERNAL(s_ActiveProject);
			return s_ActiveProject->GetConfig().Name;
		}

		static std::filesystem::path GetProjectDirectory()
		{
			FROST_ASSERT_INTERNAL(s_ActiveProject);
			return s_ActiveProject->GetConfig().ProjectDirectory;
		}

		static std::filesystem::path GetAssetDirectory()
		{
			FROST_ASSERT_INTERNAL(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory)
										/ s_ActiveProject->GetConfig().AssetDirectory;
		}

		static std::filesystem::path GetAssetRegistryPath()
		{
			FROST_ASSERT_INTERNAL(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory)
										/ s_ActiveProject->GetConfig().AssetRegistryPath;
		}

		static std::filesystem::path GetScriptModulePath()
		{
			FROST_ASSERT_INTERNAL(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory)
										/ s_ActiveProject->GetConfig().ScriptModulePath;
		}

		static std::filesystem::path GetCacheDirectory()
		{
			FROST_ASSERT_INTERNAL(s_ActiveProject);
			return std::filesystem::path(s_ActiveProject->GetConfig().ProjectDirectory) / "Cache";
		}

		static Ref<Project> Deserialize(const std::string& filepath);
	private:
		void Serialize();

	private:
		ProjectConfig m_Config;

		inline static Ref<Project> s_ActiveProject;
	};

}