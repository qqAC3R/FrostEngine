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
		static void RemoveTextureCustomSlot(uint32_t slot);
	private:
		static void AddTextureInternal(uint32_t slot, const Ref<Texture2D>& texture2d);
	private:
		static std::unordered_map<uint32_t, WeakRef<Texture2D>> m_TextureSlots;

		// When we delete a texture, we actually can't delete it from the descriptor residency,
		// instead what we do is "act" like we deleted it.
		// In the HashMap of m_TextureSlots we delete it, but in the descriptor it remains (we can't delete it),
		// and so we make a new queue named "m_EmptyTextureSlots", which basically after deletion of a texture
		// adds the deleted slot there, and when we add a new texture, we firstly check this queue
		// and if we find a used slot, we reuse it.
		// This can improve the performance, because previously what I've done is just pick a random slot
		// everytime an addition happens and so the descriptor residency can have a lot of textures
		// and a lot of them would be useless (because they were deleted previously and just stay there).
		static std::queue<uint32_t> m_EmptyTextureSlots;

		static VkDescriptorPool m_DescriptorPool;
		static VkDescriptorSetLayout m_DescriptorSetLayout;
		static HashMap<uint32_t, VkDescriptorSet> m_DescriptorSet;

		static uint32_t m_DescriptorSetNumber;
		static uint32_t m_TextureMaxStorage;
		static uint32_t m_DefaultTextureID;
	};

}