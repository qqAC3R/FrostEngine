#include "frostpch.h"
#include "Material.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"

namespace Frost
{

	Ref<Material> Material::Create(const Ref<Shader>& shader, const std::string& name)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanMaterial>(shader, name);
			case RendererAPI::API::OpenGL: return nullptr;
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}