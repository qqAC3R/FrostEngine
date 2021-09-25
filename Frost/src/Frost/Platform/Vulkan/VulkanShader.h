#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

#include "Frost/Renderer/Shader.h"

namespace Frost
{

	class VulkanShader : public Shader
	{
	public:
		VulkanShader(const std::string& filepath);
		virtual ~VulkanShader();

		virtual const std::string& GetName() const override { return m_Name; }
		virtual void Destroy() override;

		virtual ShaderReflectionData GetShaderReflectionData() const override { return m_ReflectionData; }
		virtual std::unordered_map<uint32_t, VkDescriptorSetLayout> GetVulkanDescriptorSetLayout() const override { return m_DescriptorSetLayouts; }

		static VkShaderStageFlagBits GetShaderStageBits(ShaderType type);
	private:
		void CompileVulkanBinaries(std::unordered_map<ShaderType, std::string> shaderSources);
		void CreateShaderModules();
		void CreateVulkanDescriptorSetLayout();
		bool IsFiledChanged();
	private:
		std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_ShaderModules;
		std::unordered_map<ShaderType, std::string> m_ShaderSources;
		std::unordered_map<ShaderType, std::vector<uint32_t>> m_VulkanSPIRV;
		std::unordered_map<uint32_t, VkDescriptorSetLayout> m_DescriptorSetLayouts;
		
		std::string m_Filepath;
		std::string m_Name;

		ShaderReflectionData m_ReflectionData;

		friend class VulkanPipeline;
		friend class VulkanRayTracingPipeline;
		friend class VulkanComputePipeline;
		friend class VulkanShaderBindingTable;

		friend class VulkanMaterial;

	public:

		virtual void Bind() const override {}
		virtual void Unbind() const override {}

		virtual void UploadUniformInt(const std::string& name, int values) override {}

		virtual void UploadUniformFloat(const std::string& name, float values) override {}
		virtual void UploadUniformFloat2(const std::string& name, const glm::vec2& values) override {}
		virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& values) override {}
		virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& values) override {}

		virtual void UploadUniformMat3(const std::string& name, const glm::mat3& matrix) override {}
		virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override {}

	};


}