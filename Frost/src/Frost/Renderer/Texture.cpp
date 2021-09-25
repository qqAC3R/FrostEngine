#include "frostpch.h"
#include "Texture.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"

namespace Frost
{
	
	Ref<Texture2D> Texture2D::Create(const std::string& path, TextureSpecs spec /*= {}*/)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanTexture2D>(path, spec);
			case RendererAPI::API::OpenGL: return nullptr;
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<TextureCubeMap> TextureCubeMap::Create(TextureSpecs spec /*= {}*/)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
		case RendererAPI::API::Vulkan: return CreateRef<VulkanTextureCubeMap>(spec);
		case RendererAPI::API::OpenGL: return nullptr;
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Image2D> Image2D::Create(TextureSpecs spec /*= {}*/)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanImage2D>(spec);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}