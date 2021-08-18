#include "frostpch.h"
#include "ShaderLibrary.h"

namespace Frost
{

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		auto& name = shader->GetName();
		Add(name, shader);
	}

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		FROST_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;

	}

	void ShaderLibrary::Load(const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);

		Add(shader);
		//return shader;
	}

	Ref<Shader> ShaderLibrary::Get(const std::string& name)
	{
		FROST_ASSERT(Exists(name), "Shader not found!");
		return m_Shaders[name];
	}

	bool ShaderLibrary::Exists(const std::string& name) const
	{
		return m_Shaders.find(name) != m_Shaders.end();
	}

	void ShaderLibrary::Clear()
	{
		for (auto& shader : m_Shaders)
		{
			shader.second->Destroy();
		}
	}

}