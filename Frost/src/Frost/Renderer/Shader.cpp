#include "frostpch.h"
#include "Shader.h"

#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Utils/Timer.h"

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

	Ref<Shader> Shader::Create(const std::string& filepath, const Vector<ShaderArray>& customMemberArraySizes)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanShader>(filepath, customMemberArraySizes);
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "Haven't added OpenGL yet!"); return nullptr;
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	static std::string GetShaderNameFromFilepath(const std::string& filepath)
	{
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind(".");
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;

		return filepath.substr(lastSlash, count);
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
		std::string shaderName = GetShaderNameFromFilepath(filepath);
		std::string text = "Shader '" + shaderName + "' was compiled in ";
		Timer shaderCompileTimer(text.c_str());

		auto shader = Shader::Create(filepath);
		Add(shader);
	}

	void ShaderLibrary::Load(const std::string& filepath, const Vector<ShaderArray>& customMemberArraySizes)
	{
		auto shader = Shader::Create(filepath, customMemberArraySizes);
		auto& name = shader->GetName();

		FROST_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;
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


	namespace Utils
	{
		ShaderBufferData::Member::Type SPVTypeToShaderDataType(const spirv_cross::SPIRType& spvType);
	};

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
				//ShaderBufferData& uniformBuffer = m_BufferData[resource.name];
				ShaderBufferData uniformBuffer;
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				uniformBuffer.Type = ShaderBufferData::BufferType::Uniform;
				uniformBuffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uniformBuffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uniformBuffer.ShaderStage.push_back(stage);
				uniformBuffer.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				uniformBuffer.Name = resource.name;
				uniformBuffer.Count = typeID.array[0] == 0 ? 1 : typeID.array[0]; // If the member is an array

				// Members
				for (uint32_t i = 0; i < bufferType.member_types.size(); i++)
				{
					const spirv_cross::SPIRType& spvType = compiler.get_type(bufferType.member_types[i]);

					ShaderBufferData::Member bufferMember;
					std::string memberName = resource.name + '.' + compiler.get_member_name(bufferType.self, i);
					bufferMember.MemoryOffset = compiler.type_struct_member_offset(bufferType, i); // In bytes
					bufferMember.DataType = Utils::SPVTypeToShaderDataType(spvType);
					uniformBuffer.Members[memberName] = bufferMember;
				}

				// Push back to a vector
				m_BufferVectorData.push_back(std::make_pair(resource.name, uniformBuffer));
			}

			// Storage buffers
			for (const auto& resource : resources.storage_buffers)
			{
				//ShaderBufferData& storageBuffer = m_BufferData[resource.name];
				ShaderBufferData storageBuffer;
				const auto& bufferType = compiler.get_type(resource.base_type_id);
				const auto& typeID = compiler.get_type(resource.type_id);

				storageBuffer.Type = ShaderBufferData::BufferType::Storage;
				storageBuffer.Set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				storageBuffer.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				storageBuffer.ShaderStage.push_back(stage);
				storageBuffer.Size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				storageBuffer.Name = resource.name;
				storageBuffer.Count = typeID.array[0] == 0 ? 1 : typeID.array[0]; // If the member is an array

				// Members
				for (uint32_t i = 0; i < bufferType.member_types.size(); i++)
				{
					const spirv_cross::SPIRType& spvType = compiler.get_type(bufferType.member_types[i]);

					ShaderBufferData::Member bufferMember;
					std::string memberName = resource.name + '.' + compiler.get_member_name(bufferType.self, i);
					bufferMember.MemoryOffset = compiler.type_struct_member_offset(bufferType, i); // In bytes
					bufferMember.DataType = Utils::SPVTypeToShaderDataType(spvType);
					storageBuffer.Members[memberName] = bufferMember;
				}

				m_BufferVectorData.push_back(std::make_pair(resource.name, storageBuffer));
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
		int maxDescriptorSetNr = -1;

		// Adding all the sets used in the shader needed to make the amount of descriptor sets
		for (auto& buffer : m_BufferData)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(m_DescriptorSetsCount.begin(), m_DescriptorSetsCount.end(), buffer.second.Set) == m_DescriptorSetsCount.end())
			{
				// Getting the highest descriptor set number from the whole shader (checking every type member: buffers, textures, acceleration structures)
				int bufferSet = static_cast<int>(buffer.second.Set);
				if (bufferSet > maxDescriptorSetNr) maxDescriptorSetNr = buffer.second.Set;

				m_DescriptorSetsCount.push_back(buffer.second.Set);
			}
		}

		for (auto& texture : m_TextureData)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(m_DescriptorSetsCount.begin(), m_DescriptorSetsCount.end(), texture.Set) == m_DescriptorSetsCount.end())
			{
				// Getting the highest descriptor set number from the whole shader (checking every type member: buffers, textures, acceleration structures)
				int textureSet = static_cast<int>(texture.Set);
				if (textureSet > maxDescriptorSetNr) maxDescriptorSetNr = texture.Set;

				m_DescriptorSetsCount.push_back(texture.Set);
			}
		}

		for (auto& accelerationStructure : m_AccelerationStructureData)
		{
			// Check if the number of the set is already mentioned in the vector
			if (std::find(m_DescriptorSetsCount.begin(), m_DescriptorSetsCount.end(), accelerationStructure.Set) == m_DescriptorSetsCount.end())
			{
				// Getting the highest descriptor set number from the whole shader (checking every type member: buffers, textures, acceleration structures)
				int accelerationStructureSet = static_cast<int>(accelerationStructure.Set);
				if (accelerationStructureSet > maxDescriptorSetNr) maxDescriptorSetNr = accelerationStructure.Set;

				m_DescriptorSetsCount.push_back(accelerationStructure.Set);
			}
		}

		if (maxDescriptorSetNr >= 0)
		{
			m_DescriptorSetMax = maxDescriptorSetNr;
		}
		else
		{
			m_DescriptorSetMax = UINT32_MAX;
		}

	}

	void ShaderReflectionData::ClearRepeatedMembers()
	{
		// Just a simple O(n^2) algorithm to find the repeated members


		// Buffers
		for (uint32_t i = 0; i < m_BufferVectorData.size(); i++)
		{
			for (uint32_t j = i + 1; j < m_BufferVectorData.size(); j++)
			{
				auto& bufferData1 = m_BufferVectorData[i].second;
				auto& bufferData2 = m_BufferVectorData[j].second;

				// Check if the binding and set of the buffers is the same
				if (bufferData1.Set == bufferData2.Set && bufferData1.Binding == bufferData2.Binding)
				{
					// If the binding and set of the buffer is the same, then we add the shaderstages from the second buffer into the first one
					// (so like we combine them), and then erase the second buffer so we have only one combined
					bufferData1.ShaderStage.push_back(bufferData2.ShaderStage[0]);

					// Deleting the second copy of the same member
					m_BufferVectorData.erase(m_BufferVectorData.begin() + j);
				}
			}
		}

		// Hashing the buffer data
		for (auto&& [name, buffer] : m_BufferVectorData)
		{
			m_BufferData[name] = buffer;
		}


		// Textures
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

	namespace Utils
	{
		ShaderBufferData::Member::Type SPVTypeToShaderDataType(const spirv_cross::SPIRType& spvType)
		{
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 1 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::Float;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 2 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::Float2;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 3 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::Float3;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 4 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::Float4;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 2 && spvType.columns == 2)
			{
				return ShaderBufferData::Member::Type::Mat2;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 3 && spvType.columns == 3)
			{
				return ShaderBufferData::Member::Type::Mat3;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Float && spvType.vecsize == 4 && spvType.columns == 4)
			{
				return ShaderBufferData::Member::Type::Mat4;
			}
			if (spvType.basetype == spirv_cross::SPIRType::UInt && spvType.vecsize == 1 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::UInt;
			}
			if (spvType.basetype == spirv_cross::SPIRType::UInt64 && spvType.vecsize == 1 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::UInt64;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Int && spvType.vecsize == 1 && spvType.columns == 1)
			{
				return ShaderBufferData::Member::Type::Int;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Boolean)
			{
				return ShaderBufferData::Member::Type::Bool;
			}
			if (spvType.basetype == spirv_cross::SPIRType::Struct)
			{
				return ShaderBufferData::Member::Type::Struct;
			}

			FROST_ASSERT_MSG("No spirv_cross::SPIRType matches Surge::ShaderDataType!");
			return ShaderBufferData::Member::Type::None;
		}
	};

}