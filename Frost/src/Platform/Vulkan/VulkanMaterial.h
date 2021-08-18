#pragma once

#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/Material.h"

namespace Frost
{

	class VulkanMaterial : public Material
	{
	public:
		VulkanMaterial(const Ref<Shader>& shader, const std::string& name);
		virtual ~VulkanMaterial();

		virtual void UpdateVulkanDescriptor() override;
		virtual void Bind(void* cmdBuf, VkPipelineLayout pipelineLayout, GraphicsType graphicsType) const override;

		virtual Vector<VkDescriptorSetLayout> GetVulkanDescriptorLayout() const override {
			Vector<VkDescriptorSetLayout> descriptorSetLayouts;
			for (auto& descriptorSetLayout : m_DescriptorSetLayouts)
				descriptorSetLayouts.push_back(descriptorSetLayout.second);
			return descriptorSetLayouts;
		}

		virtual void Invalidate() override;

		virtual void Set(const std::string& name, float value) override;
		virtual void Set(const std::string& name, int value) override;
		virtual void Set(const std::string& name, glm::vec2 value) override;
		virtual void Set(const std::string& name, glm::vec3 value) override;
		virtual void Set(const std::string& name, glm::vec4 value) override;
		virtual void Set(const std::string& name, glm::mat3 value) override;
		virtual void Set(const std::string& name, glm::mat4 value) override;

		virtual void Set(const std::string& name, const Ref<Texture2D>& texture) override;
		virtual void Set(const std::string& name, const Ref<Image2D>& image) override;
		
		virtual void Set(const std::string& name, const Ref<Buffer>& storageBuffer) override;
		virtual void Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer) override;

		virtual void Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure) override;


		// TODO: Do getters
		virtual float GetFloat(const std::string& name) override;
		virtual int GetInt(const std::string& name) override;
		virtual glm::vec2 GetVector2(const std::string& name) override;
		virtual glm::vec3 GetVector3(const std::string& name) override;
		virtual glm::vec4 GetVector4(const std::string& name) override;
		virtual glm::mat3 GetMatrix3(const std::string& name) override;
		virtual glm::mat4 GetMatrix4(const std::string& name) override;


		virtual Ref<Buffer> GetBuffer(const std::string& name) override;
		virtual Ref<UniformBuffer> GetUniformBuffer(const std::string& name) override;
		virtual Ref<Texture2D> GetTexture2D(const std::string& name) override;
		virtual Ref<Image2D> GetImage2D(const std::string& name) override;
		virtual Ref<TopLevelAccelertionStructure> GetAccelerationStructure(const std::string& name) override;


		virtual void Destroy() override;

	private:
		std::pair<uint32_t, uint32_t> GetShaderLocationFromString(const std::string& name);
		void CreateVulkanDescriptor();
		void CreateMaterialData();

	private:
		Ref<Shader> m_Shader;

		VkDescriptorPool m_DescriptorPool;
		std::unordered_map<uint32_t, VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::unordered_map<uint32_t, VkDescriptorSet> m_DescriptorSets;

		struct DataPointer
		{
			enum class DataType {
				BUFFER, TEXTURE, ACCELERATION_STRUCTURE
			};

			// DataType is for a bit of validation, so for example we could assert if we set a uniform buffer into texture
			DataType Type;
			Ref<void*> Pointer;
		};
		std::unordered_map<std::string, DataPointer> m_MaterialData;


		ShaderReflectionData m_ReflectedData;

		struct PendingDescriptor
		{
			// Keeping a reference of every object to not get deleted before the descriptor set is updated (at binding)
			Ref<Image2D> Image;
			Ref<Texture2D> Texture;
			Ref<Buffer> StorageBuffer;
			Ref<UniformBuffer> UniformBuffer;
			Ref<TopLevelAccelertionStructure> AccelerationStructure;

			VkWriteDescriptorSet WDS{};
		};

		// Keeping them because in a vector because they fucking die and crashes the whole engine
		Vector<VkDescriptorImageInfo> ImageInfos;
		Vector<VkDescriptorBufferInfo> BufferInfos;
		Vector<VkWriteDescriptorSetAccelerationStructureKHR> ASInfos;

		Vector<PendingDescriptor> m_PendingDescriptor;



	};

}