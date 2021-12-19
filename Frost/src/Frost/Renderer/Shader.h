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
		RayGen, AnyHit, ClosestHit, Miss, Intersection,

		// Additional gpu driven shaders
		Fragment_Bindless, Compute_Bindless, ClosestHit_Bindless
	};

	class ShaderReflectionData;
	struct ShaderArray;

	class Shader
	{
	public:
		~Shader() = default;
		
		virtual const std::string& GetName() const = 0;
		virtual ShaderReflectionData GetShaderReflectionData() const = 0;
		virtual void Destroy() = 0;

		/* OpenGL Specific API */
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
		
		/* Vulkan Specific API */ //TODO: This should be inside the vk implemetation
		virtual std::unordered_map<uint32_t, VkDescriptorSetLayout> GetVulkanDescriptorSetLayout() const = 0;
		
		static Ref<Shader> Create(const std::string& filepath);
		static Ref<Shader> Create(const std::string& filepath, const Vector<ShaderArray>& customMemberArraySizes);
	};

	class ShaderLibrary
	{
	public:
		ShaderLibrary();
		~ShaderLibrary();

		void Add(const Ref<Shader>& shader);
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Load(const std::string& filepath);
		void Load(const std::string& filepath, const Vector<ShaderArray>& customMemberArraySizes);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;

		void Clear();
	private:
		HashMap<std::string, Ref<Shader>> m_Shaders;
	};

	struct ShaderArray
	{
		ShaderArray() = default;
		ShaderArray(const std::string& name, uint32_t size)
			: Name(name), Size(size)
		{
		}

		// If the size is UINT32_MAX that means the descriptor doesn't have a size
		// For example:  `layout(set = 0, binding = 0) sampler2D textures[];`
		ShaderArray(const std::string& name)
			: Name(name), Size(UINT32_MAX)
		{
		}

		std::string Name;
		uint32_t Size = 0;
	};

	struct ShaderBufferData
	{
		enum class BufferType {
			Uniform, Storage
		};
		BufferType Type;

		struct Member
		{
			enum class Type
			{
				None, Int = 4, UInt = 4, UInt64 = 8, Float = 4, Float2 = 8, Float3 = 12, Float4 = 16, Mat2 = 16, Mat3 = 36, Mat4 = 64, Bool = 4, Struct
			};
			uint32_t MemoryOffset = 0;
			Type DataType = Type::None;
		};
		HashMap<std::string, Member> Members;

		uint32_t Set;
		uint32_t Binding;
		uint32_t Size;
		uint32_t Count;
		Vector<ShaderType> ShaderStage;
		std::string Name;
	};

	struct ShaderTextureData
	{
		enum class TextureType {
			Sampled, Storage
		};
		TextureType Type;
		uint32_t Set;
		uint32_t Binding;
		uint32_t Count;
		Vector<ShaderType> ShaderStage;
		std::string Name;
	};

	struct ShaderAccelerationStructureData
	{
		uint32_t Set;
		uint32_t Binding;
		uint32_t Count;
		Vector<ShaderType> ShaderStage;
		std::string Name;
	};

	struct PushConstantData
	{
		uint32_t Size;
		Vector<ShaderType> ShaderStage;
		std::string Name;
	};

	class ShaderReflectionData
	{
	public:
		ShaderReflectionData();
		virtual ~ShaderReflectionData();

		void SetReflectionData(std::unordered_map<ShaderType, std::vector<uint32_t>> reflectionData);

		const HashMap<std::string, ShaderBufferData>& GetBuffersData() const { return m_BufferData; }
		const Vector<ShaderTextureData>& GetTextureData() const { return m_TextureData; }
		const Vector<ShaderAccelerationStructureData>& GetAccelerationStructureData() const { return m_AccelerationStructureData; }
		const Vector<PushConstantData>& GetPushConstantData() const { return m_PushConstantData; }

		const Vector<uint32_t> GetDescriptorSetsCount() const { return m_DescriptorSetsCount; }
		const uint32_t GetDescriptorSetMax() const { return m_DescriptorSetMax; }
	private:
		void SetDescriptorSetsCount();
		void ClearRepeatedMembers();
	private:
		HashMap<std::string, ShaderBufferData> m_BufferData;
		Vector<ShaderTextureData> m_TextureData;
		Vector<ShaderAccelerationStructureData> m_AccelerationStructureData;
		Vector<PushConstantData> m_PushConstantData;

		Vector<std::pair<std::string, ShaderBufferData>> m_BufferVectorData;

		Vector<uint32_t> m_DescriptorSetsCount;
		uint32_t m_DescriptorSetMax;
	};
}