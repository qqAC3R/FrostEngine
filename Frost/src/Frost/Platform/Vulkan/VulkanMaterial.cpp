#include "frostpch.h"
#include "VulkanMaterial.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanUniformBuffer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanRayTracingPipeline.h"

namespace Frost
{

	namespace Utils
	{
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

	// A HUGE descriptor pool for allocating descriptor sets
	static VkDescriptorPool s_DescriptorPool = VK_NULL_HANDLE;

	VulkanMaterial::VulkanMaterial(const Ref<Shader>& shader, const std::string& name)
		: m_Shader(shader)
	{
		Ref<VulkanShader> vulkanShader = m_Shader.As<VulkanShader>();
		m_DescriptorSetLayouts = vulkanShader->m_DescriptorSetLayouts;

		m_ReflectedData = m_Shader->GetShaderReflectionData();
		CreateVulkanDescriptor();
		CreateMaterialData();
	}

	VulkanMaterial::~VulkanMaterial()
	{
		m_MaterialData.clear();
	}

	void VulkanMaterial::AllocateDescriptorPool()
	{
		if (s_DescriptorPool != VK_NULL_HANDLE) return;

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		Vector<VkDescriptorPoolSize> poolSizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 10000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10000 },
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 10000 }
		};
		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = 10000 * poolSizes.size();
		poolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
		poolCreateInfo.pPoolSizes = poolSizes.data();
		FROST_VKCHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &s_DescriptorPool));
	}

	void VulkanMaterial::DeallocateDescriptorPool()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyDescriptorPool(device, s_DescriptorPool, nullptr);
	}

	void VulkanMaterial::UpdateVulkanDescriptorIfNeeded()
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
	}

	void VulkanMaterial::Bind(Ref<Pipeline> pipeline) 
	{
		UpdateVulkanDescriptorIfNeeded();

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
		VkPipelineLayout pipelineLayout = vulkanPipeline->GetVulkanPipelineLayout();

		// Typically we have 1-5 descriptor sets and doing a for loop for everyframe wont hurt the performance
		Vector<VkDescriptorSet> descriptorSets;
		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSets.push_back(descriptorSet.second);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}

	void VulkanMaterial::Bind(Ref<ComputePipeline> computePipeline)
	{
		FROST_ASSERT_MSG("Binding to a compute pipeline needs a command buffer!");
	}

	void VulkanMaterial::Bind(VkCommandBuffer cmdBuf, Ref<ComputePipeline> computePipeline)
	{
		UpdateVulkanDescriptorIfNeeded();

		Ref<VulkanComputePipeline> vulkanComputePipeline = computePipeline.As<VulkanComputePipeline>();
		VkPipelineLayout pipelineLayout = vulkanComputePipeline->GetVulkanPipelineLayout();

		// Typically we have 1-5 descriptor sets and doing a for loop for everyframe wont hurt the performance
		Vector<VkDescriptorSet> descriptorSets;
		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSets.push_back(descriptorSet.second);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}

	void VulkanMaterial::Bind(Ref<RayTracingPipeline> rayTracingPipeline)
	{
		UpdateVulkanDescriptorIfNeeded();

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanRayTracingPipeline> vulkanPipeline = rayTracingPipeline.As<VulkanRayTracingPipeline>();
		VkPipelineLayout pipelineLayout = vulkanPipeline->GetVulkanPipelineLayout();

		// Typically we have 1-5 descriptor sets and doing a for loop for everyframe wont hurt the performance
		Vector<VkDescriptorSet> descriptorSets;
		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSets.push_back(descriptorSet.second);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}


#if 0
	void VulkanMaterial::Bind() const
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		
		VkPipelineBindPoint vkGraphicsType = VulkanContext::GetVulkanGraphicsType(m_PipelineGraphicsType);
		VkCommandBuffer cmdBuf;
		if (m_PipelineGraphicsType == GraphicsType::Graphics || m_PipelineGraphicsType == GraphicsType::Raytracing)
		{
			cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		}
		else if (m_PipelineGraphicsType == GraphicsType::Compute)
		{
			cmdBuf = VulkanContext::GetSwapChain()->GetComputeCommandBuffer(currentFrameIndex);
		}

		// Typically we have 1-5 descriptor sets and doing a for loop for everyframe wont hurt the performance
		Vector<VkDescriptorSet> descriptorSets;
		for (auto& descriptorSet : m_DescriptorSets)
			descriptorSets.push_back(descriptorSet.second);

		vkCmdBindDescriptorSets(cmdBuf, vkGraphicsType, m_PipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}
#endif

	void VulkanMaterial::CreateVulkanDescriptor()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		auto reflectedData = m_ReflectedData;


		if (reflectedData.GetDescriptorSetsCount().size() == 0) return;

#if 0
		///////////////////////////////////////////////////////////
		// Descriptor Pool
		///////////////////////////////////////////////////////////

		Vector<VkDescriptorPoolSize> descriptorPoolSize;
		// Getting the needed descriptor pool size for the buffers
		for (auto& buffer : reflectedData.GetBuffersData())
		{
			VkDescriptorPoolSize& dpSize = descriptorPoolSize.emplace_back();
			dpSize.type = Utils::BufferTypeToVulkan(buffer.Type);
			dpSize.descriptorCount = buffer.Count;
		}

		// Getting the needed descriptor pool size for the textures
		for (auto& texture : reflectedData.GetTextureData())
		{
			VkDescriptorPoolSize& dpSize = descriptorPoolSize.emplace_back();
			dpSize.type = Utils::TextureTypeToVulkan(texture.Type);
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

		FROST_VKCHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_DescriptorPool));

		std::string descriptorPoolName = "VulkanShader-DescriptorPool[" + m_Shader->GetName() + "]";
		VulkanContext::SetStructDebugName(descriptorPoolName, VK_OBJECT_TYPE_DESCRIPTOR_POOL, m_DescriptorPool);
#endif



		for (auto& descriptorSetNumber : reflectedData.GetDescriptorSetsCount())
		{
			///////////////////////////////////////////////////////////
			// Descriptor Set
			///////////////////////////////////////////////////////////

			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = s_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &m_DescriptorSetLayouts[descriptorSetNumber];

			FROST_VKCHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSets[descriptorSetNumber]));

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

	VulkanMaterial::ShaderLocation VulkanMaterial::GetShaderLocationFromString(const std::string& name)
	{
		auto bufferData = m_ReflectedData.GetBuffersData();
		auto textureData = m_ReflectedData.GetTextureData();
		auto asData = m_ReflectedData.GetAccelerationStructureData();

		for (auto& buffer : bufferData)
			if (buffer.Name == name)
				return { buffer.Set, buffer.Binding };

		for (auto& texture : textureData)
			if (texture.Name == name)
				return { texture.Set, texture.Binding };

		for (auto& as : asData)
			if(as.Name == name)
				return { as.Set, as.Binding };

		FROST_ASSERT_MSG("Couldn't find the location of the member");
		return { UINT_MAX, UINT_MAX };
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<Buffer>& storageBuffer)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.StorageBuffer = storageBuffer;
		auto location = GetShaderLocationFromString(name);

		if (m_MaterialData[name].Type != DataPointer::DataType::BUFFER) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = storageBuffer.As<void*>();


		VkDescriptorBufferInfo* bufferInfo = &storageBuffer.As<VulkanBuffer>()->GetVulkanDescriptorInfo();

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSet.pBufferInfo = bufferInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];

	}

	void VulkanMaterial::Set(const std::string& name, const Ref<UniformBuffer>& uniformBuffer)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.UniformBuffer = uniformBuffer;
		auto location = GetShaderLocationFromString(name);

		if (m_MaterialData[name].Type != DataPointer::DataType::BUFFER) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = uniformBuffer.As<void*>();

		VkDescriptorBufferInfo* bufferInfo = &uniformBuffer.As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo();

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pBufferInfo = bufferInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];
	}


	void VulkanMaterial::Set(const std::string& name, const Ref<Texture2D>& texture)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.Texture = texture;
		auto location = GetShaderLocationFromString(name);

		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT_MSG("Wrong data type!");
		m_MaterialData[name].Pointer = texture.As<void*>();


		Ref<VulkanTexture2D> vulkanImage2d = texture.As<VulkanTexture2D>();
		VkDescriptorImageInfo* imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pImageInfo = imageDescriptorInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];

		UpdateVulkanDescriptorIfNeeded();
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<Image2D>& image)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.Image = image;
		auto location = GetShaderLocationFromString(name);
		ShaderTextureData::TextureType shaderTextureType = ShaderTextureData::TextureType::Storage;

		// Getting how the Image2D is being used by the shader (as sampled/storage)
		for (auto& texture : m_ReflectedData.GetTextureData())
			if (location.Set == texture.Set && location.Binding == texture.Binding)
				shaderTextureType = texture.Type;

		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = image.As<void*>();


		Ref<VulkanImage2D> vulkanImage2d = image.As<VulkanImage2D>();
		VkDescriptorImageInfo* imageDescriptorInfo;

		if (shaderTextureType == ShaderTextureData::TextureType::Storage)
			imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Storage);
		else
			imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);
		

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = shaderTextureType == ShaderTextureData::TextureType::Storage ?
											VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		writeDescriptorSet.pImageInfo = imageDescriptorInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<TextureCubeMap>& image)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.Image = image;
		auto location = GetShaderLocationFromString(name);
		ShaderTextureData::TextureType shaderTextureType = ShaderTextureData::TextureType::Storage;

		// Getting how the Image2D is being used by the shader (as sampled/storage)
		for (auto& texture : m_ReflectedData.GetTextureData())
			if (location.Set == texture.Set && location.Binding == texture.Binding)
				shaderTextureType = texture.Type;

		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = image.As<void*>();

		auto imageCubeMap = image.As<VulkanTextureCubeMap>();
		VkDescriptorImageInfo* imageDescriptorInfo;

		if (shaderTextureType == ShaderTextureData::TextureType::Storage)
			imageDescriptorInfo = &imageCubeMap->GetVulkanDescriptorInfo(DescriptorImageType::Storage);
		else
			imageDescriptorInfo = &imageCubeMap->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = shaderTextureType == ShaderTextureData::TextureType::Storage ?
			VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		writeDescriptorSet.pImageInfo = imageDescriptorInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<TopLevelAccelertionStructure>& accelerationStructure)
	{
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		pendingDescriptor.AccelerationStructure = accelerationStructure;
		auto location = GetShaderLocationFromString(name);
		auto vkAccelerationStructure = accelerationStructure.As<VulkanTopLevelAccelertionStructure>();


		if (m_MaterialData[name].Type != DataPointer::DataType::ACCELERATION_STRUCTURE) FROST_ASSERT(false, "Wrong data type!");
		m_MaterialData[name].Pointer = accelerationStructure.As<void*>();

		
		VkWriteDescriptorSetAccelerationStructureKHR* asCreateInfo = &accelerationStructure.As<VulkanTopLevelAccelertionStructure>()->GetVulkanDescriptorInfo();

		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writeDescriptorSet.pNext = asCreateInfo;
		writeDescriptorSet.descriptorCount = 1;

		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];

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
		//m_MaterialData.clear();
	}
}