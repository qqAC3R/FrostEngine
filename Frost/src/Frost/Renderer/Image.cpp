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

		FROST_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}

	Ref<Image2D> Image2D::Create(const ImageSpecification& specification, const void* data)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
		case RendererAPI::API::Vulkan: return CreateRef<VulkanImage2D>(specification, data);
		}

		FROST_ASSERT(false, "Unknown RendererAPI");
		return nullptr;
	}
}