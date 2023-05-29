#pragma once

#include "Frost/Renderer/Texture.h"

namespace Frost
{

	// Forward declaration
	class Pipeline;
	class ComputePipeline;
	class RayTracingPipeline;

	class BindlessAllocator
	{
	public:
		BindlessAllocator();
		virtual ~BindlessAllocator();

		static void Init();
		static void ShutDown();

		static void Bind(Ref<Pipeline> pipeline);
		static void Bind(Ref<ComputePipeline> computePipeline);
		static void Bind(Ref<RayTracingPipeline> rayTracingPipeline);

		static uint32_t GetDescriptorSetNumber() { return m_DescriptorSetNumber; }
		static uint32_t GetMaxTextureStorage() { return m_TextureMaxStorage; }
		static uint32_t GetWhiteTextureID() { return m_DefaultTextureID; }

		static uint32_t AddTexture(const Ref<Texture2D>& texture2d);
		static void AddTextureCustomSlot(const Ref<Texture2D>& texture2d, uint32_t slot);
		static void RemoveTextureCustomSlot(uint32_t slot);
	private:
		static const uint32_t m_DescriptorSetNumber = 1;
		static const uint32_t m_DefaultTextureID = 0; // For white texture
		static const uint32_t m_TextureMaxStorage = 2048; // 2048
	};

}