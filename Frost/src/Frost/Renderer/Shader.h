#pragma once

#include <glm/glm.hpp>

using VkDescriptorSetLayout = struct VkDescriptorSetLayout_T*;

namespace Frost
{

	enum class ShaderType
	{
		None = 0,

		// Rasterization
		Vertex, Fragment, Geometry, Tessalation,

		Compute,

		// Ray Tracing
		RayGen, AnyHit, ClosestHit, Miss, Intersection

	};

	class ShaderReflectionData;

	class Shader
	{
	public:
		~Shader() = default;
		
		virtual const std::string& GetName() const = 0;
		virtual ShaderReflectionData GetShaderReflectionData() = 0;
		virtual void Destroy() = 0;

		/* OpenGL Specific API */
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void UploadUniformInt(const std::string& name, int values) = 0;

		virtual void UploadUniformFloat(const std::string& name, float values) = 0;
		virtual void UploadUniformFloat2(const std::string& name, const glm::vec2& values) = 0;
		virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& values) = 0;
		virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& values) = 0;

		virtual void UploadUniformMat3(const std::string& name, const glm::mat3& matrix) = 0;
		virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;

		
		/* Vulkan Specific API */
		virtual std::unordered_map<uint32_t, VkDescriptorSetLayout> GetVulkanDescriptorSetLayout() const = 0;

		
		static Ref<Shader> Create(const std::string& filepath);
	};


	struct BufferData
	{
		enum class BufferType {
			Uniform, Storage
		};

		uint32_t Set;
		uint32_t Binding;

		uint32_t Size;

		BufferType Type;
		Vector<ShaderType> ShaderStage;

		uint32_t Count;

		std::string Name;
	};

	struct TextureData
	{
		enum class TextureType {
			Sampled, Storage
		};

		uint32_t Set;
		uint32_t Binding;

		TextureType Type;
		Vector<ShaderType> ShaderStage;

		uint32_t Count;

		std::string Name;
	};

	struct AccelerationStructureData
	{
		uint32_t Set;
		uint32_t Binding;

		Vector<ShaderType> ShaderStage;
		uint32_t Count;

		std::string Name;
	};

	class ShaderReflectionData
	{
	public:
		ShaderReflectionData();

		void SetReflectionData(std::unordered_map<ShaderType, std::vector<uint32_t>> reflectionData);

		const Vector<BufferData>& GetBuffersData() const { return m_BufferData; }
		const Vector<TextureData>& GetTextureData() const { return m_TextureData; }
		const Vector<AccelerationStructureData>& GetAccelerationStructureData() const { return m_AccelerationStructureData; }

		const Vector<uint32_t> GetDescriptorSetsCount() const { return m_DescriptorSetsCount; }

	private:
		void SetDescriptorSetsCount();
		void ClearRepeatedMembers();

	private:
		Vector<BufferData> m_BufferData;
		Vector<TextureData> m_TextureData;
		Vector<AccelerationStructureData> m_AccelerationStructureData;

		Vector<uint32_t> m_DescriptorSetsCount;
	};
}