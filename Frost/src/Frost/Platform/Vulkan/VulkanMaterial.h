#pragma once

#include "Frost/Core/Buffer.h"
#include "Frost/Renderer/Material.h"
#include "Frost/Platform/Vulkan/Vulkan.h"

namespace Frost
{
	class VulkanMaterial : public Material
	{
	public:
		VulkanMaterial(const Ref<Shader>& shader, const std::string& name);
		virtual ~VulkanMaterial();

		virtual void Bind(Ref<Pipeline> pipeline) override;
		virtual void Bind(Ref<ComputePipeline> computePipeline) override;
		virtual void Bind(Ref<RayTracingPipeline> rayTracingPipeline) override;
		void Bind(VkCommandBuffer cmdBuf, Ref<ComputePipeline> computePipeline);

		Vector<VkDescriptorSetLayout> GetVulkanDescriptorLayout() const {
			Vector<VkDescriptorSetLayout> descriptorSetLayouts;
			for (auto& descriptorSetLayout : m_DescriptorSetLayouts)
				descriptorSetLayouts.push_back(descriptorSetLayout.second);
			return descriptorSetLayouts;
		}
		VkDescriptorSet GetVulkanDescriptorSet(uint32_t set) { return m_DescriptorSets[set]; }
		Vector<VkDescriptorSet> GetVulkanDescriptorSets() { return m_CachedDescriptorSets; }

		virtual void Set(const std::string& name, float value) override;
		virtual void Set(const std::string& name, uint32_t value) override;
		virtual void Set(const std::string& name, const glm::vec3& value) override;
		virtual void Set(const std::string& name, const Ref<Texture2D>& texture) override;
		virtual void Set(const std::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex) override;
		virtual void Set(const std::string& name, const Ref<Image2D>& image) override;
		virtual void Set(const std::string& name, const Ref<TextureCubeMap>& cubeMap) override;
		virtual void Set(const std::string& name, const Ref<BufferDevice>& storageBuffer) override;
		virtual void Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer) override;
		virtual void Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure) override;

		virtual Ref<BufferDevice> GetBuffer(const std::string& name) override;
		virtual Ref<UniformBuffer> GetUniformBuffer(const std::string& name) override;
		virtual Ref<Texture2D> GetTexture2D(const std::string& name) override;
		virtual Ref<Image2D> GetImage2D(const std::string& name) override;
		virtual Ref<TopLevelAccelertionStructure> GetAccelerationStructure(const std::string& name) override;

		virtual float& GetFloat(const std::string& name) override;
		virtual uint32_t& GetUint(const std::string& name) override;
		virtual glm::vec3& GetVector3(const std::string& name) override;

		template <typename T>
		void Set(const std::string& name, const T& value)
		{
			// Get the location of the member(offset/size)
			auto ul = GetUniformLocation(name);
			if (sizeof(T) != ul.Size) return;

			// Getting the uniform buffer as a ref
			Ref<UniformBufferData> ubData = m_MaterialData[ul.StructName].Pointer.AsRef<UniformBufferData>();

			// Writting to the cpu buffer
			Buffer& buffer = ubData->Buffer;
			buffer.Write((Byte*)&value, ul.Size, ul.Offset);

			// Copying the cpu buffer into the gpu uniform buffer
			ubData->UniformBuffer->SetData(buffer.Data);
		}

		template <typename T>
		T& Get(const std::string& name)
		{
			// Get the location of the member(offset/size)
			auto ul = GetUniformLocation(name);

			// Getting the uniform buffer as a ref
			Ref<UniformBufferData> ubData = m_MaterialData[ul.StructName].Pointer.AsRef<UniformBufferData>();

			// Read from the cpu buffer
			Buffer& buffer = ubData->Buffer;
			return buffer.Read<T>(ul.Offset);
		}

		virtual void Destroy() override;
		void UpdateVulkanDescriptorIfNeeded();
	public:
		struct ShaderLocation { uint32_t Set, Binding; };
		struct UniformLocation { uint32_t Offset, Size; std::string StructName, MemberName; };

		ShaderLocation GetShaderLocationFromString(const std::string& name);
		UniformLocation GetUniformLocation(const std::string& name);
	private:
		void CreateVulkanDescriptor();
		void CreateMaterialData();

		static void AllocateDescriptorPool();
		static void DeallocateDescriptorPool();
	private:
		Ref<Shader> m_Shader;
		ShaderReflectionData m_ReflectedData;

		HashMap<std::string, ShaderLocation> m_ShaderLocations;
		
		std::unordered_map<uint32_t, VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::unordered_map<uint32_t, VkDescriptorSet> m_DescriptorSets;
		Vector<VkDescriptorSet> m_CachedDescriptorSets;

		struct UniformBufferData
		{
			Buffer Buffer;
			Ref<UniformBuffer> UniformBuffer;
		};
		Vector<Ref<UniformBufferData>> m_UniformBuffers;

		struct DataPointer
		{
			// DataType is for a bit of validation, so for example we could assert if we set a uniform buffer into texture
			enum class DataType {
				BUFFER, TEXTURE, ACCELERATION_STRUCTURE
			};
			DataType Type;
			WeakRef<void*> Pointer;
		};
		std::unordered_map<std::string, DataPointer> m_MaterialData;

		struct PendingDescriptor
		{
			WeakRef<void*> Pointer;
			VkWriteDescriptorSet WDS{};
		};
		Vector<PendingDescriptor> m_PendingDescriptor;

		friend class VulkanRenderer;
	};
}