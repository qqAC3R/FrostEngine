#include "frostpch.h"
#include "Shader.h"

#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Renderer/Renderer.h"

#include <spirv_cross.hpp>

namespace Frost
{
	
	Ref<Shader> Shader::Create(const std::string& filepath)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanShader>(filepath);
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "Haven't added OpenGL yet!"); return nullptr;
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	ShaderLibrary::ShaderLibrary()
	{
	}

	ShaderLibrary::~ShaderLibrary()
	{
		for (auto& shader : m_Shaders)
			shader.second->Destroy();
	}

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

	ShaderReflectionData::ShaderReflectionData()
	{
	}

	ShaderReflectionData::~ShaderReflectionData()
	{
	}

	void ShaderReflectionData::SetReflectionData(std::unordered_map<ShaderType, std::vector<uint32_t>> reflectionData)
	{
		for (auto&& [stage, data] : reflectionData)
		{
			spirv_cross::Compiler compiler(data);
			spirv_cross::ShaderResources resources = compiler.get_shader_resources();

			// Uniform buffers
			for (const auto& resource : resources.uniform_buffers)
			{
				ShaderBufferData& uniformBuffer = m_BufferData.emplace_back();
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				uniformBuffer.Type = ShaderBufferData::BufferType::Uniform;
				uniformBuffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uniformBuffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uniformBuffer.ShaderStage.push_back(stage);
				uniformBuffer.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				uniformBuffer.Name = resource.name;
				uniformBuffer.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
			}

			// Storage buffers
			for (const auto& resource : resources.storage_buffers)
			{
				ShaderBufferData& storageBuffer = m_BufferData.emplace_back();
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				storageBuffer.Type = ShaderBufferData::BufferType::Storage;
				storageBuffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				storageBuffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				storageBuffer.ShaderStage.push_back(stage);
				storageBuffer.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				storageBuffer.Name = resource.name;
				storageBuffer.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
			}

			// Sampler Textures
			for (const auto& resource : resources.sampled_images)
			{
				ShaderTextureData& sampledTexture = m_TextureData.emplace_back();
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				sampledTexture.Type = ShaderTextureData::TextureType::Sampled;
				sampledTexture.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				sampledTexture.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				sampledTexture.ShaderStage.push_back(stage);
				sampledTexture.Name = resource.name;
				sampledTexture.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
			}

			// Storage Textures
			for (const auto& resource : resources.storage_images)
			{
				ShaderTextureData& storageTexture = m_TextureData.emplace_back();
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				storageTexture.Type = ShaderTextureData::TextureType::Storage;
				storageTexture.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				storageTexture.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				storageTexture.ShaderStage.push_back(stage);
				storageTexture.Name = resource.name;
				storageTexture.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
			}

			// Push Constant buffers
			for (const auto& resource : resources.push_constant_buffers)
			{
				PushConstantData& pushConstantData = m_PushConstantData.emplace_back();
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				pushConstantData.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				pushConstantData.ShaderStage.push_back(stage);
				pushConstantData.Name = resource.name;
			}

			// Acceleration Structure
			for (const auto& resource : resources.acceleration_structures)
			{
				ShaderAccelerationStructureData& storageTexture = m_AccelerationStructureData.emplace_back();
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				storageTexture.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				storageTexture.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				storageTexture.ShaderStage.push_back(stage);
				storageTexture.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
				storageTexture.Name = resource.name;
			}

		}

		SetDescriptorSetsCount();
		ClearRepeatedMembers();
	}

	void ShaderReflectionData::SetDescriptorSetsCount()
	{
		// Adding all the sets used in the shader needed to make the amount of descriptor sets
		for (auto& buffer : m_BufferData)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(m_DescriptorSetsCount.begin(), m_DescriptorSetsCount.end(), buffer.Set) == m_DescriptorSetsCount.end())
				m_DescriptorSetsCount.push_back(buffer.Set);
		}

		for (auto& texture : m_TextureData)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(m_DescriptorSetsCount.begin(), m_DescriptorSetsCount.end(), texture.Set) == m_DescriptorSetsCount.end())
				m_DescriptorSetsCount.push_back(texture.Set);
		}

		for (auto& accelerationStructure : m_AccelerationStructureData)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(m_DescriptorSetsCount.begin(), m_DescriptorSetsCount.end(), accelerationStructure.Set) == m_DescriptorSetsCount.end())
				m_DescriptorSetsCount.push_back(accelerationStructure.Set);
		}
	}

	void ShaderReflectionData::ClearRepeatedMembers()
	{
		// Just a simple O(n^2) algorithm to find the repeated members

		for (uint32_t i = 0; i < m_BufferData.size(); i++)
		{
			for (uint32_t j = i + 1; j < m_BufferData.size(); j++)
			{
				auto& bufferData1 = m_BufferData[i];
				auto& bufferData2 = m_BufferData[j];

				// Check if the binding and set of the buffers is the same
				if (bufferData1.Set == bufferData2.Set && bufferData1.Binding == bufferData2.Binding)
				{
					// If the binding and set of the buffer is the same, then we add the shaderstages from the second buffer into the first one
					// (so like we combine them), and then erase the second buffer so we have only one combined
					bufferData1.ShaderStage.push_back(bufferData2.ShaderStage[0]);

					// Deleting the second copy of the same member
					m_BufferData.erase(m_BufferData.begin() + j);
				}
			}
		}


		for (uint32_t i = 0; i < m_TextureData.size(); i++)
		{
			for (uint32_t j = i + 1; j < m_TextureData.size(); j++)
			{
				auto& textureData1 = m_TextureData[i];
				auto& textureData2 = m_TextureData[j];

				// Check if the binding and set of the textures is the same
				if (textureData1.Set == textureData2.Set && textureData1.Binding == textureData2.Binding)
				{
					// If the binding and set of the texture is the same, then we add the shaderstages from the second texture into the first one
					// (so like we combine them), and then erase the second texture so we have only one combined
					textureData1.ShaderStage.push_back(textureData2.ShaderStage[0]);

					// Deleting the second copy of the same member
					m_TextureData.erase(m_TextureData.begin() + j);
				}
			}
		}


		for (uint32_t i = 0; i < m_AccelerationStructureData.size(); i++)
		{
			for (uint32_t j = i + 1; j < m_AccelerationStructureData.size(); j++)
			{
				auto& ASData1 = m_AccelerationStructureData[i];
				auto& ASData2 = m_AccelerationStructureData[j];

				// Check if the binding and set of the AStructures is the same
				if (ASData1.Set == ASData2.Set && ASData1.Binding == ASData2.Binding)
				{
					// If the binding and set of the AStructure is the same, then we add the shaderstages from the second AStructure into the first one
					// (so like we combine them), and then erase the second AStructure so we have only one combined
					ASData1.ShaderStage.push_back(ASData2.ShaderStage[0]);

					// Deleting the second copy of the same member
					m_AccelerationStructureData.erase(m_AccelerationStructureData.begin() + j);
				}
			}
		}

		// For Push Constants
		for (uint32_t i = 0; i < m_PushConstantData.size(); i++)
		{
			for (uint32_t j = i + 1; j < m_PushConstantData.size(); j++)
			{
				auto& pushConstant1 = m_PushConstantData[i];
				auto& pushConstant2 = m_PushConstantData[j];

				// Check if the size of the push constants are the same
				if (pushConstant1.Size == pushConstant2.Size && pushConstant1.Name == pushConstant2.Name)
				{
					// If the size of the push constants are the same, then we add the shaderstages from the second push constant buffer into the first one
					// (so like we combine them), and then erase the second push constant buffer so we have only one combined |Explanation 100|
					for (auto shaderStages : pushConstant2.ShaderStage)
						pushConstant1.ShaderStage.push_back(shaderStages);

					// Deleting the second copy of the same member
					m_PushConstantData.erase(m_PushConstantData.begin() + j);
				}
			}
		}

	}


}