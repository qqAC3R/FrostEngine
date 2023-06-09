#include "frostpch.h"
#include "VulkanBindlessAllocator.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"

#include "Frost/Platform/Vulkan/RayTracing/VulkanRayTracingPipeline.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"

#include <random>

namespace Frost
{
#if 0
	// Static members
	std::unordered_map<uint32_t, WeakRef<Texture2D>> VulkanBindlessAllocator::m_TextureSlots;
	std::queue<uint32_t> VulkanBindlessAllocator::m_EmptyTextureSlots;
#endif

	// Get the global variables from "BindlessAllocator.cpp"
	extern std::unordered_map<uint32_t, WeakRef<Texture2D>> m_TextureSlots;
	extern std::queue<uint32_t> m_EmptyTextureSlots;

	// Vulkan specific
	HashMap<uint32_t, VkDescriptorSet> VulkanBindlessAllocator::m_DescriptorSet;
	VkDescriptorSetLayout VulkanBindlessAllocator::m_DescriptorSetLayout;
	VkDescriptorPool VulkanBindlessAllocator::m_DescriptorPool;

	// std
	static std::random_device s_RandomDevice;
	static std::mt19937_64 s_Engine(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

	VulkanBindlessAllocator::VulkanBindlessAllocator()
	{
	}

	VulkanBindlessAllocator::~VulkanBindlessAllocator()
	{
	}

	void VulkanBindlessAllocator::Init()
	{
		// Random generator initialization
		s_UniformDistribution = std::uniform_int_distribution<uint64_t>(1, BindlessAllocator::GetMaxTextureStorage() - 1);

		///////////////////////////////////////////////////////////
		// Descriptor Pool
		///////////////////////////////////////////////////////////
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t textureCount = BindlessAllocator::GetMaxTextureStorage() * framesInFlight; // ('m_TextureMaxStorage'  textures) * ('framesInFlight' texture sets)

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		Vector<VkDescriptorPoolSize> poolSizes =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount},
		};
		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = framesInFlight * poolSizes.size();
		poolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
		poolCreateInfo.pPoolSizes = poolSizes.data();
		FROST_VKCHECK(vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &m_DescriptorPool));

		///////////////////////////////////////////////////////////
		// Descriptor Set Layout
		///////////////////////////////////////////////////////////
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding = 0;
		layoutBinding.descriptorCount = textureCount / framesInFlight;
		layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

		Vector<VkDescriptorBindingFlags> descriptorBindingFlags = { VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT };

		VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlags{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
		descriptorSetLayoutBindingFlags.bindingCount = (uint32_t)descriptorBindingFlags.size();
		descriptorSetLayoutBindingFlags.pBindingFlags = descriptorBindingFlags.data();

		VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutInfo.pNext = &descriptorSetLayoutBindingFlags;
		layoutInfo.flags = 0;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &layoutBinding;
		FROST_VKCHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_DescriptorSetLayout));
		VulkanContext::SetStructDebugName("VulkanShader-DescriptorSetLayout[Bindless]", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, m_DescriptorSetLayout);



		///////////////////////////////////////////////////////////
		// Descriptor Set
		///////////////////////////////////////////////////////////
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			uint32_t descriptorSetNumber = i;

			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &m_DescriptorSetLayout;

			FROST_VKCHECK(vkAllocateDescriptorSets(device, &allocInfo, &m_DescriptorSet[descriptorSetNumber]));

			std::string descriptorSetName = "VulkanShader-DescriptorSet-Bindless";
			VulkanContext::SetStructDebugName(descriptorSetName, VK_OBJECT_TYPE_DESCRIPTOR_SET, m_DescriptorSet[descriptorSetNumber]);
		}
	}

	uint32_t VulkanBindlessAllocator::AddTexture(const Ref<Texture2D>& texture2d)
	{
		uint32_t descriptorSetLocation = UINT32_MAX;

#if 0
		// Check firstly if we have empty slots from previous deleted texture, if so then reuse them!
		// ( We can use the term - Recycle :D )
		if (!m_EmptyTextureSlots.empty())
		{
			uint32_t emptyTextureSlot = m_EmptyTextureSlots.front();
			AddTextureCustomSlot(texture2d, emptyTextureSlot);
			m_EmptyTextureSlots.pop();

			FROST_CORE_WARN(emptyTextureSlot);

			return emptyTextureSlot;
		}
#endif

		// While loop till we find a empty texture slot
		while (true)
		{
			// Generate a random number
			descriptorSetLocation = s_UniformDistribution(s_Engine);

			// If we found a texture with the same slot, then make some conditions
			if (m_TextureSlots.find(descriptorSetLocation) != m_TextureSlots.end())
			{
				// If the texture slot that we chose randomly was using an outdated (nullptr) texture, then we can replace it with a new one
				if (!m_TextureSlots[descriptorSetLocation].IsValid())
				{
					m_TextureSlots[descriptorSetLocation] = texture2d;

					if (texture2d.Raw() != nullptr)
					{
						AddTextureInternal(descriptorSetLocation, texture2d);
					}

					return descriptorSetLocation;
				}
				else
				{
					// If the texture slot is occupied and it is also valid, then choose another random number
					continue;
				}
			}
			else
			{
				// If we didnt find any textures in this slot, then just occupy it 
				m_TextureSlots[descriptorSetLocation] = texture2d;
				if (texture2d.Raw() != nullptr)
				{
					AddTextureInternal(descriptorSetLocation, texture2d);
				}

				return descriptorSetLocation;
			}

		}

		FROST_ASSERT_MSG("Something is wrong here!");
		return UINT32_MAX;
	}

	void VulkanBindlessAllocator::AddTextureCustomSlot(const Ref<Texture2D>& texture2d, uint32_t slot)
	{
		m_TextureSlots[slot] = texture2d;
		if (texture2d.Raw() != nullptr)
		{
			AddTextureInternal(slot, texture2d);
		}
	}

	void VulkanBindlessAllocator::RemoveTextureCustomSlot(uint32_t slot)
	{
		if (slot == BindlessAllocator::GetWhiteTextureID()) return; // We cannot delete the default texture

		m_TextureSlots[slot] = nullptr;
		m_TextureSlots.erase(slot);
	}

	void VulkanBindlessAllocator::AddTextureInternal(uint32_t slot, const Ref<Texture2D>& texture2d)
	{
		// Get the descriptor info
		Ref<VulkanTexture2D> vulkanImage2d = texture2d.As<VulkanTexture2D>();
		VkDescriptorImageInfo* imageDescriptorInfo = &vulkanImage2d->GetVulkanDescriptorInfo(DescriptorImageType::Sampled);

		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDeviceWaitIdle(device); // I wanted to avoid this with some hacky tricks, but at some point the validation errors were like "fuck you", so i had no choice :(

		for (auto& descriptorSet : m_DescriptorSet)
		{
			// Push a WDS into pending descriptors
			VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writeDescriptorSet.dstBinding = 0;
			writeDescriptorSet.dstArrayElement = slot;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeDescriptorSet.pImageInfo = imageDescriptorInfo;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = descriptorSet.second;

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
	}

	void VulkanBindlessAllocator::ShutDown()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyDescriptorSetLayout(device, m_DescriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
	}

	void VulkanBindlessAllocator::Bind(Ref<Pipeline> pipeline)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanPipeline> vulkanPipeline = pipeline.As<VulkanPipeline>();
		VkPipelineLayout pipelineLayout = vulkanPipeline->GetVulkanPipelineLayout();

		VkDescriptorSet descriptorSet = m_DescriptorSet[currentFrameIndex];
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}

	void VulkanBindlessAllocator::Bind(Ref<ComputePipeline> computePipeline)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanComputePipeline> vulkanComputePipeline = computePipeline.As<VulkanComputePipeline>();
		VkPipelineLayout pipelineLayout = vulkanComputePipeline->GetVulkanPipelineLayout();

		VkDescriptorSet descriptorSet = m_DescriptorSet[currentFrameIndex];
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}

	void VulkanBindlessAllocator::Bind(Ref<RayTracingPipeline> rayTracingPipeline)
	{ 
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Ref<VulkanRayTracingPipeline> vulkanPipeline = rayTracingPipeline.As<VulkanRayTracingPipeline>();
		VkPipelineLayout pipelineLayout = vulkanPipeline->GetVulkanPipelineLayout();

		VkDescriptorSet descriptorSet = m_DescriptorSet[currentFrameIndex];
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	}

}