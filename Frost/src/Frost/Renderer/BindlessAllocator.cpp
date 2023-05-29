#include "frostpch.h"
#include "BindlessAllocator.h"

#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"

namespace Frost
{

	/// Global variables
	std::unordered_map<uint32_t, WeakRef<Texture2D>> m_TextureSlots;

	// TODO:
	// When we delete a texture, we actually can't delete it from the descriptor residency,
	// instead what we do is "act" like we deleted it.
	// In the HashMap of m_TextureSlots we delete it, but in the descriptor it remains (we can't delete it),
	// and so we make a new queue named "m_EmptyTextureSlots", which basically after deletion of a texture
	// adds the deleted slot there, and when we add a new texture, we firstly check this queue
	// and if we find a used slot, we reuse it.
	// 
	// This can improve the performance, because previously what I've done is just pick a random slot and forget about the old one.
	// If this was done multiple times it would add up to a lot of textures in the texture residency,
	// most of which would be white, since we deleted them and so the texture residency would be bigger
	// and harder for shaders to access that many texture pages at once
	std::queue<uint32_t> m_EmptyTextureSlots;


	BindlessAllocator::BindlessAllocator()
	{
	}

	BindlessAllocator::~BindlessAllocator()
	{
	}

	void BindlessAllocator::Init()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::Vulkan: VulkanBindlessAllocator::Init();
			case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

	void BindlessAllocator::ShutDown()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: VulkanBindlessAllocator::ShutDown();
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

	void BindlessAllocator::Bind(Ref<Pipeline> pipeline)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: VulkanBindlessAllocator::Bind(pipeline);
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

	void BindlessAllocator::Bind(Ref<ComputePipeline> computePipeline)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: VulkanBindlessAllocator::Bind(computePipeline);
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

	void BindlessAllocator::Bind(Ref<RayTracingPipeline> rayTracingPipeline)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: VulkanBindlessAllocator::Bind(rayTracingPipeline);
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

	uint32_t BindlessAllocator::AddTexture(const Ref<Texture2D>& texture2d)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: return VulkanBindlessAllocator::AddTexture(texture2d);
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!"); return 0;
		}
		return 0;
	}

	void BindlessAllocator::AddTextureCustomSlot(const Ref<Texture2D>& texture2d, uint32_t slot)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: VulkanBindlessAllocator::AddTextureCustomSlot(texture2d, slot);
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

	void BindlessAllocator::RemoveTextureCustomSlot(uint32_t slot)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::Vulkan: VulkanBindlessAllocator::RemoveTextureCustomSlot(slot);
		case RendererAPI::API::None: FROST_ASSERT_INTERNAL("Renderer::API::None is not supported!");
		}
	}

}