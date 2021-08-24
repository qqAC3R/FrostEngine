#include "frostpch.h"
#include "VulkanMaterial.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanShader.h"
#include "Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

namespace Frost
{

	namespace VulkanUtils
	{
		static VkDescriptorType BufferTypeToVulkan(BufferData::BufferType type)
		{
			switch (type)
			{
				case BufferData::BufferType::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				case BufferData::BufferType::Storage: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			}

			FROST_ASSERT(false, "");
			return VkDescriptorType();
		}

		static VkDescriptorType TextureTypeToVulkan(TextureData::TextureType type)
		{
			switch (type)
			{
				case TextureData::TextureType::Sampled: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				case TextureData::TextureType::Storage:	return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
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



	VulkanMaterial::VulkanMaterial(const Ref<Shader>& shader, const std::string& name)
		: m_Shader(shader)
	{
		Invalidate();
	}

	void VulkanMaterial::Invalidate()
	{
		Ref<VulkanShader> vulkanShader = m_Shader.As<VulkanShader>();
		m_DescriptorSetLayouts = vulkanShader->m_DescriptorSetLayouts;

		m_ReflectedData = m_Shader->GetShaderReflectionData();
		CreateVulkanDescriptor();
		CreateMaterialData();
	}

	VulkanMaterial::~VulkanMaterial()
	{
	}

	void VulkanMaterial::UpdateVulkanDescriptor()
	{
		if (!m_PendingDescriptor.size()) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		Vector<VkWriteDescriptorSet> writeDescriptorSet;
		for (auto& descriptorInfo : m_PendingDescriptor)
		{
			writeDescriptorSet.push_back(descriptorInfo.WDS);
		}

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSet.size()), writeDescriptorSet.data(), 0, nullptr);

		m_PendingDescriptor.clear();

		ASInfos.clear();
		ImageInfos.clear();
		BufferInfos.clear();
	}

	void VulkanMaterial::Bind(void* cmdBuf, VkPipelineLayout pipelineLayout, GraphicsType graphicsType) const
	{
		VkCommandBuffer vkCmdBuf = (VkCommandBuffer)cmdBuf;
		auto vkGraphicsType = VulkanContext::GetVulkanGraphicsType(graphicsType);

		Vector<VkDescriptorSet> descriptorSets;
		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSets.push_back(descriptorSet.second);

		vkCmdBindDescriptorSets(vkCmdBuf, vkGraphicsType, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}

	void VulkanMaterial::CreateVulkanDescriptor()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		auto reflectedData = m_ReflectedData;


		if (reflectedData.GetDescriptorSetsCount().size() == 0) return;

		///////////////////////////////////////////////////////////
		// Descriptor Pool
		///////////////////////////////////////////////////////////

		Vector<VkDescriptorPoolSize> descriptorPoolSize;
		// Getting the needed descriptor pool size for the buffers
		for (auto& buffer : reflectedData.GetBuffersData())
		{
			VkDescriptorPoolSize& dpSize = descriptorPoolSize.emplace_back();
			dpSize.type = VulkanUtils::BufferTypeToVulkan(buffer.Type);
			dpSize.descriptorCount = buffer.Count;
		}

		// Getting the needed descriptor pool size for the textures
		for (auto& texture : reflectedData.GetTextureData())
		{
			VkDescriptorPoolSize& dpSize = descriptorPoolSize.emplace_back();
			dpSize.type = VulkanUtils::TextureTypeToVulkan(texture.Type);
			dpSize.descriptorCount = texture.Count;
		}

		// Getting the needed descriptor pool size for the acceleration structures
		for (auto& accelerationStructure : reflectedData.GetAccelerationStructureData())
		{
			VkDescriptorPoolSize& dpSize = descriptorPoolSize.emplace_back();
			dpSize.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			dpSize.descriptorCount = accelerationStructure.Count;
		}

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSize.size());
		poolInfo.pPoolSizes = descriptorPoolSize.data();
		poolInfo.maxSets = (uint32_t)reflectedData.GetDescriptorSetsCount().size();

		FROST_VKCHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool), "Failed to create a descriptor pool!");

		std::string descriptorPoolName = "VulkanShader-DescriptorPool[" + m_Shader->GetName() + "]";
		VulkanContext::SetStructDebugName(descriptorPoolName, VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_DescriptorPool);




		for (auto& descriptorSetNumber : reflectedData.GetDescriptorSetsCount())
		{
			///////////////////////////////////////////////////////////
			// Descriptor Set
			///////////////////////////////////////////////////////////

			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &m_DescriptorSetLayouts[descriptorSetNumber];

			FROST_VKCHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSets[descriptorSetNumber]), "Failed to create a descriptor set!");

			std::string descriptorSetName = "VulkanShader-DescriptorSet[" + m_Shader->GetName() + "]";
			VulkanContext::SetStructDebugName(descriptorSetName, VK_OBJECT_TYPE_DESCRIPTOR_SET, m_DescriptorSets[descriptorSetNumber]);
		}

	}

	void VulkanMaterial::CreateMaterialData()
	{
		for (auto& buffer : m_ReflectedData.GetBuffersData())
		{
			m_MaterialData[buffer.Name].Pointer = nullptr;
			m_MaterialData[buffer.Name].Type = DataPointer::DataType::BUFFER;
		}

		for (auto& texture : m_ReflectedData.GetTextureData())
		{
			m_MaterialData[texture.Name].Pointer = nullptr;
			m_MaterialData[texture.Name].Type = DataPointer::DataType::TEXTURE;
		}

		for (auto& accelerationStructure : m_ReflectedData.GetAccelerationStructureData())
		{
			m_MaterialData[accelerationStructure.Name].Pointer = nullptr;
			m_MaterialData[accelerationStructure.Name].Type = DataPointer::DataType::ACCELERATION_STRUCTURE;
		}
	}

	std::pair<uint32_t, uint32_t> VulkanMaterial::GetShaderLocationFromString(const std::string& name)
	//   pair<set, binding>
	{
		auto bufferData = m_ReflectedData.GetBuffersData();
		auto textureData = m_ReflectedData.GetTextureData();
		auto asData = m_ReflectedData.GetAccelerationStructureData();

		for (auto& buffer : bufferData)
			if (buffer.Name == name)
				return std::make_pair(buffer.Set, buffer.Binding);

		for (auto& texture : textureData)
			if (texture.Name == name)
				return std::make_pair(texture.Set, texture.Binding);

		for (auto& as : asData)
			if(as.Name == name)
				return std::make_pair(as.Set, as.Binding);



		FROST_ASSERT(false, "");
		return std::make_pair(0, 0);
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<Buffer>& storageBuffer)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.StorageBuffer = storageBuffer;
		auto location = GetShaderLocationFromString(name);

		if (m_MaterialData[name].Type != DataPointer::DataType::BUFFER) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = storageBuffer.As<void*>();


		VkDescriptorBufferInfo& bufferInfo = BufferInfos.emplace_back();
		bufferInfo.buffer = storageBuffer->GetVulkanBuffer();
		bufferInfo.offset = 0;
		bufferInfo.range = storageBuffer->GetBufferSize();

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.second;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSet.pBufferInfo = &bufferInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.first];

	}

	void VulkanMaterial::Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.UniformBuffer = uniformBuffer;
		auto location = GetShaderLocationFromString(name);


		if (m_MaterialData[name].Type != DataPointer::DataType::BUFFER) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = uniformBuffer.As<void*>();

		VkDescriptorBufferInfo& bufferInfo = BufferInfos.emplace_back();
		bufferInfo.buffer = uniformBuffer->GetVulkanBuffer();
		bufferInfo.offset = 0;
		bufferInfo.range = uniformBuffer->GetBufferSize();

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.second;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = &bufferInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.first];

	}


	void VulkanMaterial::Set(const std::string& name, const Ref<Texture2D>& texture)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.Texture = texture;
		auto location = GetShaderLocationFromString(name);

		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = texture.As<void*>();

		VkDescriptorImageInfo& imageInfo = ImageInfos.emplace_back();
		imageInfo.imageLayout = texture->GetVulkanImageLayout();
		imageInfo.imageView = texture->GetVulkanImageView();
		imageInfo.sampler = texture->GetVulkanSampler();

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.second;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pImageInfo = &imageInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.first];

	}

	void VulkanMaterial::Set(const std::string& name, const Ref<Image2D>& image)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.Image = image;
		auto location = GetShaderLocationFromString(name);
		TextureData::TextureType shaderTextureType = TextureData::TextureType::Storage;

		// Getting how the Image2D is being used by the shader (as sampled/storage)
		for (auto& texture : m_ReflectedData.GetTextureData())
			if (location.first == texture.Set && location.second == texture.Binding)
				shaderTextureType = texture.Type;

		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = image.As<void*>();


		VkDescriptorImageInfo& imageInfo = ImageInfos.emplace_back();
		imageInfo.imageLayout = image->GetVulkanImageLayout();
		imageInfo.imageView = image->GetVulkanImageView();

		if (shaderTextureType == TextureData::TextureType::Sampled)
			imageInfo.sampler = image->GetVulkanSampler();
		else if (shaderTextureType == TextureData::TextureType::Storage)
			imageInfo.sampler = nullptr;



		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.second;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = shaderTextureType == TextureData::TextureType::Storage ?
											VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		writeDescriptorSet.pImageInfo = &imageInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.first];

	}

	void VulkanMaterial::Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.AccelerationStructure = accelerationStructure;
		auto location = GetShaderLocationFromString(name);
		auto vkAccelerationStructure = accelerationStructure.As<VulkanTopLevelAccelertionStructure>();


		if (m_MaterialData[name].Type != DataPointer::DataType::ACCELERATION_STRUCTURE) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = accelerationStructure.As<void*>();

		
		VkWriteDescriptorSetAccelerationStructureKHR& asCreateInfo = ASInfos.emplace_back();
		asCreateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		asCreateInfo.pNext = nullptr;
		asCreateInfo.accelerationStructureCount = 1;
		asCreateInfo.pAccelerationStructures = &vkAccelerationStructure->GetVulkanAccelerationStructure();
		

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.second;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writeDescriptorSet.pNext = &asCreateInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.first];

	}

	Ref<Buffer> VulkanMaterial::GetBuffer(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.As<Buffer>();
	}

	Ref<UniformBuffer> VulkanMaterial::GetUniformBuffer(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.As<UniformBuffer>();
	}

	Ref<Texture2D> VulkanMaterial::GetTexture2D(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.As<Texture2D>();
	}

	Ref<Image2D> VulkanMaterial::GetImage2D(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.As<Image2D>();
	}

	Ref<TopLevelAccelertionStructure> VulkanMaterial::GetAccelerationStructure(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.As<TopLevelAccelertionStructure>();
	}

	void VulkanMaterial::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		if (m_ReflectedData.GetDescriptorSetsCount().size() != 0)
			vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
	}

}