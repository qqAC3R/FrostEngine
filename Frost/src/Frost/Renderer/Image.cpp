#include "frostpch.h"
#include "Image.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"

namespace Frost
{
	Ref<Image2D> Image2D::Create(const ImageSpecification& specification)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
		case RendererAPI::API::Vulkan: return CreateRef<VulkanImage2D>(specification);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	Ref<Image2D> Image2D::Create(const ImageSpecification& specification, const void* data)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
		case RendererAPI::API::Vulkan: return CreateRef<VulkanImage2D>(specification, data);
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

	uint32_t Utils::CalculateMipMapLevels(uint32_t width, uint32_t height)
	{
		return (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
	}

}