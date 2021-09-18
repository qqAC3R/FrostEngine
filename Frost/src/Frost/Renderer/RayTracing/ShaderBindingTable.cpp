#include "frostpch.h"
#include "ShaderBindingTable.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanShaderBindingTable.h"

#include "Frost/Core/Application.h"
#include "Frost/Math/Alignment.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Platform/Vulkan/VulkanShader.h"

namespace Frost
{
	Ref<ShaderBindingTable> ShaderBindingTable::Create(const Ref<Shader>& shader)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "OpenGL::API doesn't support RayTracing!");
			case RendererAPI::API::Vulkan: return Ref<VulkanShaderBindingTable>::Create(shader);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}