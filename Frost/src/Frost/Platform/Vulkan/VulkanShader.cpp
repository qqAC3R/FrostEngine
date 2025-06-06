#include "frostpch.h"
#include "VulkanShader.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"

#include <filesystem>
#include <shaderc/shaderc.hpp>
#include <spirv-tools/optimizer.hpp>

#include <json/nlohmann/json.hpp>

#include "Frost/Utils/Timer.h"

namespace Frost
{
	
	namespace Utils
	{
		static const char* GetShaderCacheDirectory();
		static const char* GetShaderHashCacheDirectory();
		static void CreateCacheDirectoryIfNeeded();
		static std::string GetNameFromFilepath(const std::string& filepath);
		static std::string ReadFile(const std::string& filename);
		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code);
		VkShaderStageFlagBits ShaderTypeToStage(ShaderType type);
		std::string ShaderTypeToString(ShaderType type);
		static const char* ShaderStageCachedFileExtension(ShaderType stage);
		static shaderc_shader_kind ShaderStageToShaderC(ShaderType stage);
		static ShaderType ShaderTypeFromString(const std::string& type);
		std::unordered_map<ShaderType, std::string> PreProccesShaders(const std::string& source);
		static VkDescriptorType BufferTypeToVulkan(ShaderBufferData::BufferType type);
		static VkDescriptorType TextureTypeToVulkan(ShaderTextureData::TextureType type);
		static VkShaderStageFlags GetShaderStagesFlagsFromShaderTypes(Vector<ShaderType> shaderTypes);
	}

	VulkanShader::VulkanShader(const std::string& filepath)
		: m_Filepath(filepath)
	{
		m_Name = Utils::GetNameFromFilepath(filepath);

		Utils::CreateCacheDirectoryIfNeeded();
		std::string source = Utils::ReadFile(filepath);
		auto shaderSources = Utils::PreProccesShaders(source);
		m_ShaderSources = shaderSources;
		m_ShaderSource = source;

		// Check if we use bindless in a shader
		{
			for (auto& shaderSource : m_ShaderSources)
			{
				switch (shaderSource.first)
				{
				case ShaderType::Fragment_Bindless:
				case ShaderType::Compute_Bindless:
				case ShaderType::ClosestHit_Bindless:
					m_UseBindless = true; break;
				default:
					m_UseBindless = false;
				}

				if (m_UseBindless) break;
			}
		}

#if 0
		{
			Timer shaderCompileTimer("VulkanShader");
		}
#endif
		CompileVulkanBinaries(shaderSources);
		CreateShaderModules();

		m_ReflectionData.SetReflectionData(m_VulkanSPIRV);
		CreateVulkanDescriptorSetLayout();
	}

	VulkanShader::VulkanShader(const std::string& filepath, const Vector<ShaderArray>& customMemberArraySizes)
	{
		m_Name = Utils::GetNameFromFilepath(filepath);

		Utils::CreateCacheDirectoryIfNeeded();
		std::string source = Utils::ReadFile(filepath);
		auto shaderSources = Utils::PreProccesShaders(source);
		m_ShaderSources = shaderSources;

		// Check if we use bindless in a shader
		{
			for (auto& shaderSource : m_ShaderSources)
			{
				switch (shaderSource.first)
				{
				case ShaderType::Fragment_Bindless:
				case ShaderType::Compute_Bindless:
				case ShaderType::ClosestHit_Bindless:
					m_UseBindless = true; break;
				default:
					m_UseBindless = false;
				}

				if (m_UseBindless) break;
			}
		}

#if 0
		// Creating the shader binaries and the vulkan shader module
		{
			Timer shaderCompileTimer("VulkanShader");
		}
#endif
		CompileVulkanBinaries(shaderSources);
		CreateShaderModules();

		// Creating a hashmap for the custom sized members
		for (auto& memberArraySize : customMemberArraySizes)
			m_CustomMemberArraySizes[memberArraySize.Name] = memberArraySize.Size;

		m_ReflectionData.SetReflectionData(m_VulkanSPIRV);
		CreateVulkanDescriptorSetLayout();
	}

	VulkanShader::~VulkanShader()
	{
		Destroy();
	}

	void VulkanShader::CreateShaderModules()
	{
		for (auto& vulkanSPIRVSourceShader: m_VulkanSPIRV)
		{
			auto& shaderModule = m_ShaderModules[Utils::ShaderTypeToStage(vulkanSPIRVSourceShader.first)];

			shaderModule = Utils::CreateShaderModule(vulkanSPIRVSourceShader.second);
			VulkanContext::SetStructDebugName("ShaderModule", VK_OBJECT_TYPE_SHADER_MODULE, shaderModule);
		}
	}

	void VulkanShader::CompileVulkanBinaries(std::unordered_map<ShaderType, std::string> shaderSources)
	{
		// Setting up the options for the spriv compiler
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
		options.SetTargetSpirv(shaderc_spirv_version_1_5);
		const bool optimize = false;
		if (optimize)
			options.SetOptimizationLevel(shaderc_optimization_level_performance);
		
		std::filesystem::path cacheDirectory = Utils::GetShaderCacheDirectory();

		auto& shaderData = m_VulkanSPIRV;
		shaderData.clear();

		bool isFileChanged = IsFiledChanged();
		if(isFileChanged)
			CacheShaderSourceFile();

		for (auto&& [stage, source] : shaderSources)
		{
			std::filesystem::path shaderFilePath = m_Filepath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::ShaderStageCachedFileExtension(stage));

			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			
			if (!isFileChanged)
			{
				in.seekg(0, std::ios::end);
				auto size = in.tellg();
				in.seekg(0, std::ios::beg);

				auto& data = shaderData[stage];
				data.resize(size / sizeof(uint32_t));
				in.read((char*)data.data(), size);
			}
			else
			{
				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(source, Utils::ShaderStageToShaderC(stage), m_Filepath.c_str(), options);

				if (module.GetCompilationStatus() != shaderc_compilation_status_success)
				{
					FROST_CORE_ERROR("Error in {0} shader:\n {1}", Utils::ShaderTypeToString(stage), module.GetErrorMessage());
					FROST_ASSERT_MSG("Assertion requested!");
				}

				shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

				std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
				if (out.is_open())
				{
					auto& data = shaderData[stage];
					out.write((char*)data.data(), data.size() * sizeof(uint32_t));
					out.flush();
					out.close();
				}
			}
		}
	}


	void VulkanShader::CreateVulkanDescriptorSetLayout()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		auto reflectedData = GetShaderReflectionData();
		uint32_t bindlessDescriptorSetNumber = BindlessAllocator::GetDescriptorSetNumber();

		if (reflectedData.GetDescriptorSetMax() == UINT32_MAX) return;

		for(uint32_t i = 0; i <= reflectedData.GetDescriptorSetMax(); i++)
		{
			uint32_t descriptorSetNumber = i;

			// Get the reflection data from the shader
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			auto reflectedData = GetShaderReflectionData();

			if (m_UseBindless)
			{
				if (descriptorSetNumber == bindlessDescriptorSetNumber)
				{
					m_DescriptorSetLayouts[descriptorSetNumber] = VulkanBindlessAllocator::GetVulkanDescriptorSetLayout();
					continue;
				}
			}


			///////////////////////////////////////////////////////////
			// Descriptor Set Layout
			///////////////////////////////////////////////////////////

			Vector<VkDescriptorSetLayoutBinding> layoutBindings;
			VkDescriptorSetLayoutBindingFlagsCreateInfo* layoutBindingsEXT = nullptr;

			for (auto& buffer : reflectedData.GetBuffersData())
			{
				auto& bufferData = buffer.second;
				if (bufferData.Set != descriptorSetNumber) continue;

				// Check if we want to use a custom array size for the shader member
				uint32_t descriptorCount = bufferData.Count;
				if (m_CustomMemberArraySizes.find(bufferData.Name) != m_CustomMemberArraySizes.end())
				{
					descriptorCount = m_CustomMemberArraySizes[bufferData.Name];

#if 0
					if (layoutBindingsEXT == nullptr)
					{

						layoutBindingsEXT = new VkDescriptorSetLayoutBindingFlagsCreateInfo();
						layoutBindingsEXT->sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;

						std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
							0,
							VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
						};

						layoutBindingsEXT->bindingCount = descriptorBindingFlags.size();
						layoutBindingsEXT->pBindingFlags = descriptorBindingFlags.data();
					}
#endif
				}

				VkDescriptorSetLayoutBinding& LayoutBinding = layoutBindings.emplace_back();
				LayoutBinding.binding = bufferData.Binding;
				LayoutBinding.descriptorCount = descriptorCount;
				LayoutBinding.descriptorType = Utils::BufferTypeToVulkan(bufferData.Type);
				LayoutBinding.stageFlags = Utils::GetShaderStagesFlagsFromShaderTypes(bufferData.ShaderStage);
			}

			for (auto& texture : reflectedData.GetTextureData())
			{
				if (texture.Set != descriptorSetNumber) continue;



				// Check if we want to use a custom array size for the shader member
				uint32_t descriptorCount = texture.Count;
				if (m_CustomMemberArraySizes.find(texture.Name) != m_CustomMemberArraySizes.end())
				{
					descriptorCount = m_CustomMemberArraySizes[texture.Name];

#if 0
					if (layoutBindingsEXT == nullptr)
					{

						layoutBindingsEXT = new VkDescriptorSetLayoutBindingFlagsCreateInfo();
						layoutBindingsEXT->sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;

						std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags = {
							0,
							VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
						};

						layoutBindingsEXT->bindingCount = descriptorBindingFlags.size();
						layoutBindingsEXT->pBindingFlags = descriptorBindingFlags.data();
					}
#endif
				}

				VkDescriptorSetLayoutBinding& LayoutBinding = layoutBindings.emplace_back();
				LayoutBinding.binding = texture.Binding;
				LayoutBinding.descriptorCount = descriptorCount;
				LayoutBinding.descriptorType = Utils::TextureTypeToVulkan(texture.Type);
				LayoutBinding.stageFlags = Utils::GetShaderStagesFlagsFromShaderTypes(texture.ShaderStage);
			}

			for (auto& accelerationStructure : reflectedData.GetAccelerationStructureData())
			{
				if (accelerationStructure.Set != descriptorSetNumber) continue;

				VkDescriptorSetLayoutBinding& LayoutBinding = layoutBindings.emplace_back();
				LayoutBinding.binding = accelerationStructure.Binding;
				LayoutBinding.descriptorCount = accelerationStructure.Count;
				LayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
				LayoutBinding.stageFlags = Utils::GetShaderStagesFlagsFromShaderTypes(accelerationStructure.ShaderStage);
			}

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			//layoutInfo.pNext = &layoutBindingsEXT;
			layoutInfo.flags = 0;
			layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
			layoutInfo.pBindings = layoutBindings.data();

			FROST_VKCHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayouts[descriptorSetNumber]));
			VulkanContext::SetStructDebugName("VulkanShader-DescriptorSetLayout", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
											   m_DescriptorSetLayouts[descriptorSetNumber]);
		}

	}

	bool VulkanShader::IsFiledChanged()
	{
		uint64_t oldHashCode = 0;
		

		{
			// Hashing the new shader
			m_ShaderSourceHashCode = std::hash<std::string>{}(m_ShaderSource);
			FROST_CORE_INFO("[Shader] {0} shader hash code: {1}", m_Name, m_ShaderSourceHashCode);
		}

		{
			std::ifstream instream("Resources/Cache/Shaders/Vulkan/Hash/ShaderHashes.hash", std::ios::in | std::ios::binary);
			std::string hashFileContent;
			instream.seekg(0, std::ios::end);
			size_t size = instream.tellg();
			if (size != -1)
			{
				hashFileContent.resize(size);
				instream.seekg(0, std::ios::beg);
				instream.read(&hashFileContent[0], size);

				// Create A JSON, from the previous file for writing the new contents
				nlohmann::json j = nlohmann::json::parse(hashFileContent);

				if (j.contains(m_Name))
					oldHashCode = j[m_Name];
			}
			else
			{
				return true;
			}
			instream.close();
		}

		if (oldHashCode == m_ShaderSourceHashCode)
			return false;

		return true;
	}

	void VulkanShader::CacheShaderSourceFile()
	{
		nlohmann::json j;

		std::ifstream instream("Resources/Cache/Shaders/Vulkan/Hash/ShaderHashes.hash", std::ios::in | std::ios::binary);
		std::string hashFileContent;
		instream.seekg(0, std::ios::end);
		size_t size = instream.tellg();
		if (size != -1)
		{
			hashFileContent.resize(size);
			instream.seekg(0, std::ios::beg);
			instream.read(&hashFileContent[0], size);

			// Create A JSON, from the previous file for writing the new contents
			j = nlohmann::json::parse(hashFileContent);
		}

		j[m_Name] = m_ShaderSourceHashCode;

		std::string result = j.dump(4);

		FILE* f = nullptr;
		f = fopen("Resources/Cache/Shaders/Vulkan/Hash/ShaderHashes.hash", "w");
		if (f)
		{
			fwrite(result.c_str(), sizeof(char), result.size(), f);
			fclose(f);
		}
	}

	void VulkanShader::Destroy()
	{
		if (m_ShaderModules.size() == 0) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		for (auto shaderModule : m_ShaderModules)
			vkDestroyShaderModule(device, shaderModule.second, nullptr);
		
		for (auto descriptorSetLayout : m_DescriptorSetLayouts)
		{
			// Firstly check if the descriptor set layout is the bindless one, if it is, then don't delete it
			// because it will be deleted at the end of the program by the bindless allocator (which created it)
			if(descriptorSetLayout.second != VulkanBindlessAllocator::GetVulkanDescriptorSetLayout())
				vkDestroyDescriptorSetLayout(device, descriptorSetLayout.second, nullptr);
		}

		m_ShaderModules.clear();
	}

	VkShaderStageFlagBits VulkanShader::GetShaderStageBits(ShaderType type)
	{
		return Utils::ShaderTypeToStage(type);
	}

	namespace Utils
	{
		static const char* GetShaderCacheDirectory()
		{
			return "Resources/Cache/Shaders/Vulkan";
		}

		static const char* GetShaderHashCacheDirectory()
		{
			return "Resources/Cache/Shaders/Vulkan/Hash";
		}

		static void CreateCacheDirectoryIfNeeded()
		{
			std::string shaderCacheDirectory = GetShaderCacheDirectory();
			if (!std::filesystem::exists(shaderCacheDirectory))
				std::filesystem::create_directories(shaderCacheDirectory);

			std::string shaderHashCacheDirectory = GetShaderHashCacheDirectory();
			if (!std::filesystem::exists(shaderHashCacheDirectory))
				std::filesystem::create_directories(shaderHashCacheDirectory);
		}

		static std::string GetNameFromFilepath(const std::string& filepath)
		{
			auto lastSlash = filepath.find_last_of("/\\");
			lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
			auto lastDot = filepath.rfind(".");
			auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;

			return filepath.substr(lastSlash, count);
		}

		static std::string ReadFile(const std::string& filename)
		{
			std::string result;
			std::ifstream in(filename, std::ios::in | std::ios::binary); // ifstream closes itself due to RAII
			if (in)
			{
				in.seekg(0, std::ios::end);
				size_t size = in.tellg();
				if (size != -1)
				{
					result.resize(size);
					in.seekg(0, std::ios::beg);
					in.read(&result[0], size);
				}
				else
				{
					FROST_CORE_ERROR("Could not read from file '{0}'", filename);
				}
			}
			else
			{
				FROST_CORE_ERROR("Could not open file '{0}'", filename);
			}
			return result;
		}


		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			//OpReadClockKHR
			VkShaderModule shaderModule;
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = code.size() * 4;
			createInfo.pCode = code.data();

			FROST_VKCHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

			return shaderModule;
		}

		VkShaderStageFlagBits ShaderTypeToStage(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::Vertex:		  return VK_SHADER_STAGE_VERTEX_BIT;
			case ShaderType::Fragment_Bindless:
			case ShaderType::Fragment:		  return VK_SHADER_STAGE_FRAGMENT_BIT;
			case ShaderType::Geometry:		  return VK_SHADER_STAGE_GEOMETRY_BIT;
			case ShaderType::Tessalation:	  return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			case ShaderType::Compute_Bindless:
			case ShaderType::Compute:		  return VK_SHADER_STAGE_COMPUTE_BIT;
			case ShaderType::RayGen:		  return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			case ShaderType::AnyHit:		  return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			case ShaderType::ClosestHit_Bindless:
			case ShaderType::ClosestHit:	  return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			case ShaderType::Miss:			  return VK_SHADER_STAGE_MISS_BIT_KHR;
			case ShaderType::Intersection:	  return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			}

			return VkShaderStageFlagBits();
		}

		std::string ShaderTypeToString(ShaderType type)
		{
			switch (type)
			{
			case ShaderType::Vertex:		  return "Vertex";
			case ShaderType::Fragment_Bindless:
			case ShaderType::Fragment:		  return "Fragment";
			case ShaderType::Geometry:		  return "Geometry";
			case ShaderType::Tessalation:	  return "Tessalation";
			case ShaderType::Compute_Bindless:
			case ShaderType::Compute:		  return "Compute";
			case ShaderType::RayGen:		  return "RayGen";
			case ShaderType::AnyHit:		  return "AnyHit";
			case ShaderType::ClosestHit_Bindless:
			case ShaderType::ClosestHit:	  return "ClosestHit";
			case ShaderType::Miss:			  return "Miss";
			case ShaderType::Intersection:	  return "Intersection";
			}

			return "";
		}

		static const char* ShaderStageCachedFileExtension(ShaderType stage)
		{
			switch (stage)
			{
			case ShaderType::Vertex:		  return ".cached_vulkan.vert";
			case ShaderType::Fragment_Bindless:
			case ShaderType::Fragment:		  return ".cached_vulkan.frag";
			case ShaderType::Geometry:		  return ".cached_vulkan.geom";
			case ShaderType::Tessalation:	  return ".cached_vulkan.tess";
			case ShaderType::Compute_Bindless:
			case ShaderType::Compute:		  return ".cached_vulkan.comp";
			case ShaderType::RayGen:		  return ".cached_vulkan.rgen";
			case ShaderType::AnyHit:		  return ".cached_vulkan.ahit";
			case ShaderType::ClosestHit_Bindless:
			case ShaderType::ClosestHit:	  return ".cached_vulkan.rchit";
			case ShaderType::Miss:			  return ".cached_vulkan.rmiss";
			}

			FROST_ASSERT(false, "");
			return "";
		}

		static shaderc_shader_kind ShaderStageToShaderC(ShaderType stage)
		{
			switch (stage)
			{
			case ShaderType::Vertex:		  return shaderc_glsl_vertex_shader;
			case ShaderType::Fragment_Bindless:
			case ShaderType::Fragment:		  return shaderc_glsl_fragment_shader;
			case ShaderType::Geometry:		  return shaderc_glsl_geometry_shader;
			case ShaderType::Tessalation:	  return shaderc_glsl_tess_control_shader;
			case ShaderType::Compute_Bindless:
			case ShaderType::Compute:		  return shaderc_glsl_compute_shader;
			case ShaderType::RayGen:		  return shaderc_glsl_raygen_shader;
			case ShaderType::AnyHit:		  return shaderc_glsl_anyhit_shader;
			case ShaderType::ClosestHit_Bindless:
			case ShaderType::ClosestHit:	  return shaderc_glsl_closesthit_shader;
			case ShaderType::Miss:			  return shaderc_glsl_miss_shader;
			}

			FROST_ASSERT(false, "");
			return (shaderc_shader_kind)0;
		}

		static ShaderType ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")						return ShaderType::Vertex;
			if (type == "fragment" || type == "pixel")	return ShaderType::Fragment;

			if (type == "geometry")						return ShaderType::Geometry;
			if (type == "compute")						return ShaderType::Compute;

			if (type == "raygen")						return ShaderType::RayGen;
			if (type == "closesthit")					return ShaderType::ClosestHit;
			if (type == "miss")							return ShaderType::Miss;

			// Additional shaders
			if (type == "fragment(Frost_Bindless)")     return ShaderType::Fragment_Bindless;
			if (type == "compute(Frost_Bindless)")      return ShaderType::Compute_Bindless;
			if (type == "closesthit(Frost_Bindless)")   return ShaderType::ClosestHit_Bindless;

			return ShaderType::None;
		}

		std::unordered_map<ShaderType, std::string> PreProccesShaders(const std::string& source)
		{
			std::unordered_map<ShaderType, std::string> shaderSources;

			const char* typeToken = "#type";
			size_t typeTokenLength = strlen(typeToken);
			size_t pos = source.find(typeToken, 0); //Start of shader type declaration line
			while (pos != std::string::npos)
			{
				size_t eol = source.find_first_of("\r\n", pos); //End of shader type declaration line
				size_t begin = pos + typeTokenLength + 1; //Start of shader type name (after "#type " keyword)
				std::string type = source.substr(begin, eol - begin);

				FROST_ASSERT((int)ShaderTypeFromString(type), "Invalid shader type specified");

				size_t nextLinePos = source.find_first_not_of("\r\n", eol); //Start of shader code after shader type declaration line
				pos = source.find(typeToken, nextLinePos); //Start of next shader type declaration line

				shaderSources[ShaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
			}

			return shaderSources;
		}

		static VkDescriptorType BufferTypeToVulkan(ShaderBufferData::BufferType type)
		{
			switch (type)
			{
			case ShaderBufferData::BufferType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			case ShaderBufferData::BufferType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}

			FROST_ASSERT(false, "");
			return VkDescriptorType();
		}

		static VkDescriptorType TextureTypeToVulkan(ShaderTextureData::TextureType type)
		{
			switch (type)
			{
			case ShaderTextureData::TextureType::Sampled: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			case ShaderTextureData::TextureType::Storage:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			}

			FROST_ASSERT(false, "");
			return VkDescriptorType();
		}

		static VkShaderStageFlags GetShaderStagesFlagsFromShaderTypes(Vector<ShaderType> shaderTypes)
		{
			VkShaderStageFlags shaderStageFlags{};

			for (auto& shaderType : shaderTypes)
				shaderStageFlags |= VulkanShader::GetShaderStageBits(shaderType);

			return shaderStageFlags;
		}

	}
}