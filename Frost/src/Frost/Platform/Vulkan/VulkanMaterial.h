#pragma once

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

		virtual void Set(const std::string& name, const Ref<Texture2D>& texture) override;
		virtual void Set(const std::string& name, const Ref<Image2D>& image) override;
		virtual void Set(const std::string& name, const Ref<TextureCubeMap>& cubeMap) override;
		virtual void Set(const std::string& name, const Ref<Buffer>& storageBuffer) override;
		virtual void Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer) override;
		virtual void Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure) override;

		virtual Ref<Buffer> GetBuffer(const std::string& name) override;
		virtual Ref<UniformBuffer> GetUniformBuffer(const std::string& name) override;
		virtual Ref<Texture2D> GetTexture2D(const std::string& name) override;
		virtual Ref<Image2D> GetImage2D(const std::string& name) override;
		virtual Ref<TopLevelAccelertionStructure> GetAccelerationStructure(const std::string& name) override;

		virtual void Destroy() override;
		void UpdateVulkanDescriptorIfNeeded();
	private:
		struct ShaderLocation { uint32_t Set, Binding; };

		ShaderLocation GetShaderLocationFromString(const std::string& name);
		void CreateVulkanDescriptor();
		void CreateMaterialData();

		static void AllocateDescriptorPool();
		static void DeallocateDescriptorPool();
	private:
		Ref<Shader> m_Shader;

		std::unordered_map<uint32_t, VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::unordered_map<uint32_t, VkDescriptorSet> m_DescriptorSets;
		Vector<VkDescriptorSet> m_VectorDescriptorSets;

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


		ShaderReflectionData m_ReflectedData;

		struct PendingDescriptor
		{
			// Keeping a reference of every object to not get deleted before the descriptor set is updated (at binding)
			WeakRef<void*> Pointer;
			VkWriteDescriptorSet WDS{};
		};

		Vector<PendingDescriptor> m_PendingDescriptor;

		friend class VulkanRenderer;
	};

}