#include "frostpch.h"
#include "Texture.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"

namespace Frost
{
	Ref<Texture2D> Texture2D::Create(const std::string& filepath, TextureSpecification textureSpec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTexture2D>(filepath, textureSpec);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	Ref<TextureCubeMap> TextureCubeMap::Create(ImageSpecification imageSpec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTextureCubeMap>(imageSpec);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}
}