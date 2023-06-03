#include "frostpch.h"
#include "Project.h"

#include "Frost/Asset/AssetManager.h"
#include "Frost/Utils/FileSystem.h"

#include <json/nlohmann/json.hpp>

namespace Frost
{
	Project::Project()
	{
	}

	Project::~Project()
	{
	}

	void Project::SetActive(Ref<Project> project)
	{
		if (s_ActiveProject)
		{
			s_ActiveProject->Serialize();
			AssetManager::Shutdown();
		}

		s_ActiveProject = project;
		if (s_ActiveProject)
			AssetManager::Init();
	}

	static std::string GetNameFromFilepath(const std::string& filepath)
	{
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind(".");
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;

		return filepath.substr(lastSlash, count);
	}

	static std::string RemoveFileNameFromFilepath(const std::string& filepath)
	{
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;

		return filepath.substr(0, lastSlash);
	}

	Ref<Project> Project::Deserialize(const std::string& filepath)
	{
		Ref<Project> project = CreateRef<Project>();

		std::string content;
		std::ifstream instream(filepath);

		if (!instream.is_open())
		{
			FROST_ASSERT_MSG("Project could not open!");
		}

		// Read the file's content
		instream.seekg(0, std::ios::end);
		size_t size = instream.tellg();
		content.resize(size);

		instream.seekg(0, std::ios::beg);
		instream.read(&content[0], size);
		instream.close();

		// Parse the json file
		nlohmann::json in = nlohmann::json::parse(content);

		// Get the project name from filepath and all the other directories from the file
		project->m_Config.Name = GetNameFromFilepath(filepath);
		project->m_Config.ProjectDirectory = RemoveFileNameFromFilepath(filepath);
		project->m_Config.AssetDirectory = in["AssetDirectory"];
		project->m_Config.AssetRegistryPath = in["AssetRegistryPath"];
		project->m_Config.ScriptModulePath = in["ScriptModulePath"];
		project->m_Config.DefaultNamespace = in["DefaultNamespace"];
		project->m_Config.StartScene = in["StartScene"];
		project->m_Config.ReloadAssemblyOnPlay = in["ReloadAssemblyOnPlay"];

		return project;
	}

	void Project::Serialize()
	{
		std::string projDirectory = m_Config.ProjectDirectory + m_Config.Name + ".fproj";

		std::ofstream istream(projDirectory);
		istream.clear();

		nlohmann::ordered_json out = nlohmann::ordered_json();

		out["AssetDirectory"] = m_Config.AssetDirectory;
		out["AssetRegistryPath"] = m_Config.AssetRegistryPath;
		out["ScriptModulePath"] = m_Config.ScriptModulePath;
		out["DefaultNamespace"] = m_Config.DefaultNamespace;
		out["StartScene"] = m_Config.StartScene;
		out["ReloadAssemblyOnPlay"] = m_Config.ReloadAssemblyOnPlay;

		istream << out.dump(4);

		istream.close();
	}


}