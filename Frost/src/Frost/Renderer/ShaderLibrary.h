#pragma once

#include "Frost/Renderer/Shader.h"

namespace Frost
{

	class ShaderLibrary
	{
	public:

		void Add(const Ref<Shader>& shader);
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Load(const std::string& filepath);
		//Ref<Shader> Load(const std::string& name, const std::string& filepath, ShaderType type);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;

		void Clear();

	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};


}