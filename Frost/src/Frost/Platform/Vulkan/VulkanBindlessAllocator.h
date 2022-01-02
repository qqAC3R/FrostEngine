#pragma once

#include "Frost/Platform/Vulkan/VulkanTexture.h"

namespace Frost
{
	// Forward declaration
	class Pipeline;
	class ComputePipeline;
	class RayTracingPipeline;

	class VulkanBindlessAllocator
	{
	public:
		VulkanBindlessAllocator();
		virtual ~VulkanBindlessAllocator();

		static void Init();
		static void ShutDown();

		static void Bind(Ref<Pipeline> pipeline);
		static void Bind(Ref<ComputePipeline> computePipeline);
		static void Bind(Ref<RayTracingPipeline> rayTracingPipeline);

		static VkDescriptorSetLayout GetVulkanDescriptorSetLayout() { return m_DescriptorSetLayout; }
		static VkDescriptorSet GetVulkanDescriptorSet(uint32_t index) { return m_DescriptorSet[index]; }

		static uint32_t GetDescriptorSetNumber() { return m_DescriptorSetNumber; }
		static uint32_t GetMaxTextureStorage() { return m_TextureMaxStorage; }
		static uint32_t GetWhiteTextureID() { return m_DefaultTextureID; }

		static uint32_t AddTexture(const Ref<Texture2D>& texture2d);
		static void AddTextureCustomSlot(const Ref<Texture2D>& texture2d, uint32_t slot);
		static void RevmoveTextureCustomSlot(uint32_t slot);
	private:
		static void AddTextureInternal(uint32_t slot, const Ref<Texture2D>& texture2d);
	private:
		static std::unordered_map<uint32_t, WeakRef<Texture2D>> m_TextureSlots;

		static VkDescriptorPool m_DescriptorPool;
		static VkDescriptorSetLayout m_DescriptorSetLayout;
		static HashMap<uint32_t, VkDescriptorSet> m_DescriptorSet;

		static uint32_t m_DescriptorSetNumber;
		static uint32_t m_TextureMaxStorage;
		static uint32_t m_DefaultTextureID;
	};

}