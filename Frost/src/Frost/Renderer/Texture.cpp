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

	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height, TextureSpecification textureSpec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTexture2D>(width, height, textureSpec);
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

	Ref<Texture3D> Texture3D::Create(ImageSpecification imageSpec)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTexture3D>(imageSpec);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}