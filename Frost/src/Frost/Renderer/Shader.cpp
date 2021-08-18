#include "frostpch.h"
#include "Shader.h"

#include "Platform/Vulkan/VulkanShader.h"
#include "Frost/Renderer/RendererAPI.h"

#include <spirv_cross.hpp>

namespace Frost
{
	
	Ref<Shader> Shader::Create(const std::string& filepath)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanShader>(filepath);
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "Haven't added OpenGL yet!"); return nullptr;
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}


	ShaderReflectionData::ShaderReflectionData()
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
				BufferData& uniformBuffer = m_BufferData.emplace_back();

				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				uniformBuffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uniformBuffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

				uniformBuffer.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				uniformBuffer.ShaderStage.push_back(stage);

				uniformBuffer.Type = BufferData::BufferType::Uniform;
				uniformBuffer.Name = resource.name;

				uniformBuffer.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
			}

			// Storage buffers
			for (const auto& resource : resources.storage_buffers)
			{
				BufferData& storageBuffer = m_BufferData.emplace_back();

				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				storageBuffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				storageBuffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

				storageBuffer.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				storageBuffer.ShaderStage.push_back(stage);

				storageBuffer.Type = BufferData::BufferType::Storage;
				storageBuffer.Name = resource.name;

				storageBuffer.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];


			}

			// Sampler Textures
			for (const auto& resource : resources.sampled_images)
			{
				TextureData& sampledTexture = m_TextureData.emplace_back();

				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				sampledTexture.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				sampledTexture.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

				sampledTexture.ShaderStage.push_back(stage);

				sampledTexture.Type = TextureData::TextureType::Sampled;
				sampledTexture.Name = resource.name;

				sampledTexture.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];

			}

			// Storage Textures
			for (const auto& resource : resources.storage_images)
			{
				TextureData& storageTexture = m_TextureData.emplace_back();

				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				storageTexture.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				storageTexture.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

				storageTexture.ShaderStage.push_back(stage);

				storageTexture.Type = TextureData::TextureType::Storage;
				storageTexture.Name = resource.name;

				storageTexture.Count = typeID.array[0] == 0 ? 1 : typeID.array[0];
			}

			// Acceleration Structure
			for (const auto& resource : resources.acceleration_structures)
			{
				AccelerationStructureData& storageTexture = m_AccelerationStructureData.emplace_back();

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

				if (bufferData1.Set == bufferData2.Set && bufferData1.Binding == bufferData2.Binding)
				{
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

				if (textureData1.Set == textureData2.Set && textureData1.Binding == textureData2.Binding)
				{
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

				if (ASData1.Set == ASData2.Set && ASData1.Binding == ASData2.Binding)
				{
					ASData1.ShaderStage.push_back(ASData2.ShaderStage[0]);

					// Deleting the second copy of the same member
					m_AccelerationStructureData.erase(m_AccelerationStructureData.begin() + j);
				}
			}
		}

	}


}