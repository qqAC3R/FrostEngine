#include "frostpch.h"
#include "VulkanMaterial.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanShader.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanUniformBuffer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanRayTracingPipeline.h"

namespace Frost
{
	namespace Utils
	{
		static VkDescriptorType BufferTypeToVulkan(ShaderBufferData::BufferType type);
		static VkDescriptorType TextureTypeToVulkan(ShaderTextureData::TextureType type);
		static VkShaderStageFlags GetShaderStagesFlagsFromShaderTypes(Vector<ShaderType> shaderTypes);
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
		for (auto& uniformBuffer : m_UniformBuffers)
			uniformBuffer->Buffer.Release();

		m_MaterialData.clear();
	}

	void VulkanMaterial::AllocateDescriptorPool()
	{
		if (s_DescriptorPool != VK_NULL_HANDLE) return;

		uint32_t descriptorCount = std::pow(2, 16);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		Vector<VkDescriptorPoolSize> poolSizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER,                    descriptorCount },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     descriptorCount },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              descriptorCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              descriptorCount },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             descriptorCount },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             descriptorCount },
			{ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, descriptorCount }
		};
		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = descriptorCount * poolSizes.size();
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
			if(descriptorInfo.Pointer.IsValid())
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

		Vector<VkDescriptorSet> descriptorSets = m_CachedDescriptorSets;
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

		Vector<VkDescriptorSet> descriptorSets = m_CachedDescriptorSets;
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}

	void VulkanMaterial::Bind(Ref<RayTracingPipeline> rayTracingPipeline)
	{
		UpdateVulkanDescriptorIfNeeded();

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanRayTracingPipeline> vulkanPipeline = rayTracingPipeline.As<VulkanRayTracingPipeline>();
		VkPipelineLayout pipelineLayout = vulkanPipeline->GetVulkanPipelineLayout();

		Vector<VkDescriptorSet> descriptorSets = m_CachedDescriptorSets;
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
	}

	void VulkanMaterial::CreateVulkanDescriptor()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		auto reflectedData = m_ReflectedData;

		if (reflectedData.GetDescriptorSetsCount().size() == 0) return;

		for (auto& descriptorSetNumber : reflectedData.GetDescriptorSetsCount())
		{
			///////////////////////////////////////////////////////////
			// Descriptor Set
			///////////////////////////////////////////////////////////

			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorPool = s_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &m_DescriptorSetLayouts[descriptorSetNumber];

			FROST_VKCHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSets[descriptorSetNumber]));

			std::string descriptorSetName = "VulkanShader-DescriptorSet[" + m_Shader->GetName() + "]";
			VulkanContext::SetStructDebugName(descriptorSetName, VK_OBJECT_TYPE_DESCRIPTOR_SET, m_DescriptorSets[descriptorSetNumber]);

			m_CachedDescriptorSets.push_back(m_DescriptorSets[descriptorSetNumber]);
		}

	}

	void VulkanMaterial::CreateMaterialData()
	{
		for (auto& buffer : m_ReflectedData.GetBuffersData())
		{
			std::string bufferName = buffer.first;
			ShaderBufferData bufferData = buffer.second;

			m_MaterialData[bufferName].Pointer = nullptr;
			m_MaterialData[bufferName].Type = DataPointer::DataType::BUFFER;
			
			if (bufferData.Type == ShaderBufferData::BufferType::Uniform)
			{
				Ref<UniformBufferData> uniformBufferData = Ref<UniformBufferData>::Create();

				// Create a uniform buffer with the needed size
				uniformBufferData->UniformBuffer = UniformBuffer::Create(bufferData.Size);

				// Allocate a buffer (in the cpu) with the needed size
				Buffer bufferPointer;
				bufferPointer.Allocate(bufferData.Size);
				uniformBufferData->Buffer = bufferPointer;

				// Add the uniform buffer into a vector (not to lose the ref count)
				m_UniformBuffers.push_back(uniformBufferData);

				// Hash the ubo, for the getter functions
				m_MaterialData[bufferName].Pointer = uniformBufferData.As<void*>();


				// Update the descriptor set with the uniform buffer
				VkDescriptorBufferInfo* bufferInfo = &uniformBufferData->UniformBuffer.As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo();
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.dstBinding = bufferData.Binding;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSet.pBufferInfo = bufferInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = m_DescriptorSets[bufferData.Set];

				VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}

			m_ShaderLocations[bufferName] = { bufferData.Set, bufferData.Binding };
		}

		for (auto& texture : m_ReflectedData.GetTextureData())
		{
			m_MaterialData[texture.Name].Pointer = nullptr;
			m_MaterialData[texture.Name].Type = DataPointer::DataType::TEXTURE;

			m_ShaderLocations[texture.Name] = { texture.Set, texture.Binding };
		}

		for (auto& accelerationStructure : m_ReflectedData.GetAccelerationStructureData())
		{
			m_MaterialData[accelerationStructure.Name].Pointer = nullptr;
			m_MaterialData[accelerationStructure.Name].Type = DataPointer::DataType::ACCELERATION_STRUCTURE;

			m_ShaderLocations[accelerationStructure.Name] = { accelerationStructure.Set, accelerationStructure.Binding };
		}
	}

	VulkanMaterial::ShaderLocation VulkanMaterial::GetShaderLocationFromString(const std::string& name)
	{
		if (m_ShaderLocations.find(name) != m_ShaderLocations.end())
			return m_ShaderLocations[name];

		FROST_ASSERT_MSG("Couldn't find the location of the member");
		return { UINT_MAX, UINT_MAX };
	}

	VulkanMaterial::UniformLocation VulkanMaterial::GetUniformLocation(const std::string& name)
	{
		// Split the string in 2 parts (struct name/ member name)
		std::string structName = name.substr(0, name.find("."));
		std::string memberName = name.substr(name.find(".") + 1, name.size());

		// Get the hashmaps for the buffer info
		auto bufferData = m_ReflectedData.GetBuffersData();
		FROST_ASSERT(bool(bufferData.find(structName) != bufferData.end()), "Struct has not been found!");
		FROST_ASSERT(bool(bufferData[structName].Members.find(name) != bufferData[structName].Members.end()), "Member variable has not been found!");
		ShaderBufferData::Member member = bufferData[structName].Members[name];
		
		// Set the struct data
		UniformLocation uniformLocation;
		uniformLocation.Offset = member.MemoryOffset;
		uniformLocation.Size = (uint32_t)member.DataType;
		uniformLocation.StructName = structName;
		uniformLocation.MemberName = memberName;
		return uniformLocation;
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<BufferDevice>& storageBuffer)
	{
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);

		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::BUFFER) FROST_ASSERT(false, "Wrong data type!");
		Ref<void*> storageBufferPointer = storageBuffer.As<void*>();
		m_MaterialData[name].Pointer = storageBufferPointer;

		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = storageBufferPointer;

		// Get the descriptor info
		VkDescriptorBufferInfo* bufferInfo = &storageBuffer.As<VulkanBufferDevice>()->GetVulkanDescriptorInfo();

		// Push a WDS into pending descriptors
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
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);

		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::BUFFER) FROST_ASSERT(false, "Wrong data type!");
		Ref<void*> uniformBufferPointer = uniformBuffer.As<void*>();
		m_MaterialData[name].Pointer = uniformBufferPointer;

		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = uniformBufferPointer;

		// Get the descriptor info
		VkDescriptorBufferInfo* bufferInfo = &uniformBuffer.As<VulkanUniformBuffer>()->GetVulkanDescriptorInfo();

		// Push a WDS into pending descriptors
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
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);


		// TODO: Add texture arrays to the hash table
		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT_MSG("Wrong data type!");
		Ref<void*> texturePointer = texture.As<void*>();
		m_MaterialData[name].Pointer = texturePointer;
		
		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = texturePointer;

		// Get the descriptor info
		Ref<VulkanTexture2D> vulkanImage2d = texture.As<VulkanTexture2D>();
		VkDescriptorImageInfo* imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

		// Push a WDS into pending descriptors
		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pImageInfo = imageDescriptorInfo;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<Texture2D>& texture, uint32_t arrayIndex)
	{
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);

		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT_MSG("Wrong data type!");
		Ref<void*> texturePointer = texture.As<void*>();
		m_MaterialData[name].Pointer = texturePointer;

		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = texturePointer;

		// Get the descriptor info
		Ref<VulkanTexture2D> vulkanImage2d = texture.As<VulkanTexture2D>();
		VkDescriptorImageInfo* imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

		// Push a WDS into pending descriptors
		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = arrayIndex;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pImageInfo = imageDescriptorInfo;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];
	}

	void VulkanMaterial::Set(const std::string& name, const Ref<Image2D>& image)
	{
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);
		ShaderTextureData::TextureType shaderTextureType = ShaderTextureData::TextureType::Storage;

		// Getting how the Image2D is being used by the shader (as sampled/storage)
		for (auto& texture : m_ReflectedData.GetTextureData())
			if (location.Set == texture.Set && location.Binding == texture.Binding)
				shaderTextureType = texture.Type;

		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT(false, "Wrong data type!");
		Ref<void*> imagePointer = image.As<void*>();
		m_MaterialData[name].Pointer = imagePointer;

		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = imagePointer;

		// Get the descriptor info
		Ref<VulkanImage2D> vulkanImage2d = image.As<VulkanImage2D>();
		VkDescriptorImageInfo* imageDescriptorInfo;

		if (shaderTextureType == ShaderTextureData::TextureType::Storage)
			imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Storage);
		else
			imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);
		
		// Push a WDS into pending descriptors
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

	void VulkanMaterial::Set(const std::string& name, const Ref<TextureCubeMap>& cubeMap)
	{
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);
		ShaderTextureData::TextureType shaderTextureType = ShaderTextureData::TextureType::Storage;

		// Getting how the Image2D is being used by the shader (as sampled/storage)
		for (auto& texture : m_ReflectedData.GetTextureData())
			if (location.Set == texture.Set && location.Binding == texture.Binding)
				shaderTextureType = texture.Type;

		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::TEXTURE) FROST_ASSERT(false, "Wrong data type!");
		Ref<void*> imagePointer = cubeMap.As<void*>();
		m_MaterialData[name].Pointer = imagePointer;

		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = imagePointer;

		// Get the descriptor info
		auto imageCubeMap = cubeMap.As<VulkanTextureCubeMap>();
		VkDescriptorImageInfo* imageDescriptorInfo;

		if (shaderTextureType == ShaderTextureData::TextureType::Storage)
			imageDescriptorInfo = &imageCubeMap->GetVulkanDescriptorInfo(DescriptorImageType::Storage);
		else
			imageDescriptorInfo = &imageCubeMap->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

		// Push a WDS into pending descriptors
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
		// Allocate a PendingDescriptor and find the location (in the shader) of the member
		PendingDescriptor& pendingDescriptor = m_PendingDescriptor.emplace_back();
		auto location = GetShaderLocationFromString(name);
		auto vkAccelerationStructure = accelerationStructure.As<VulkanTopLevelAccelertionStructure>();

		// Set the data in the hash table
		if (m_MaterialData[name].Type != DataPointer::DataType::ACCELERATION_STRUCTURE) FROST_ASSERT(false, "Wrong data type!");
		Ref<void*> ASPointer = accelerationStructure.As<void*>();
		m_MaterialData[name].Pointer = ASPointer;

		// Set a pointer to the PendingDescriptor so when we update the descriptor set, we check if it is still valid
		pendingDescriptor.Pointer = ASPointer;

		// Get the descriptor info
		VkWriteDescriptorSetAccelerationStructureKHR* asCreateInfo = &accelerationStructure.As<VulkanTopLevelAccelertionStructure>()->GetVulkanDescriptorInfo();

		// Push a WDS into pending descriptors
		VkWriteDescriptorSet& writeDescriptorSet = pendingDescriptor.WDS;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstBinding = location.Binding;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		writeDescriptorSet.pNext = asCreateInfo;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.dstSet = m_DescriptorSets[location.Set];
	}

	void VulkanMaterial::Set(const std::string& name, const glm::vec3& value)
	{
		Set<glm::vec3>(name, value);
	}

	void VulkanMaterial::Set(const std::string& name, uint32_t value)
	{
		Set<uint32_t>(name, value);
	}

	void VulkanMaterial::Set(const std::string& name, float value)
	{
		Set<float>(name, value);
	}

	Ref<BufferDevice> VulkanMaterial::GetBuffer(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.AsRef<BufferDevice>();
	}

	Ref<UniformBuffer> VulkanMaterial::GetUniformBuffer(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.AsRef<UniformBuffer>();
	}

	Ref<Texture2D> VulkanMaterial::GetTexture2D(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.AsRef<Texture2D>();
	}

	Ref<Image2D> VulkanMaterial::GetImage2D(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.AsRef<Image2D>();
	}

	Ref<TopLevelAccelertionStructure> VulkanMaterial::GetAccelerationStructure(const std::string& name)
	{
		FROST_ASSERT(bool(m_MaterialData.find(name) != m_MaterialData.end()), "Couldn't find the member");
		return m_MaterialData[name].Pointer.AsRef<TopLevelAccelertionStructure>();
	}

	float& VulkanMaterial::GetFloat(const std::string& name)
	{
		return Get<float>(name);
	}

	uint32_t& VulkanMaterial::GetUint(const std::string& name)
	{
		return Get<uint32_t>(name);
	}

	glm::vec3& VulkanMaterial::GetVector3(const std::string& name)
	{
		return Get<glm::vec3>(name);
	}

	void VulkanMaterial::Destroy()
	{
	}

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
}