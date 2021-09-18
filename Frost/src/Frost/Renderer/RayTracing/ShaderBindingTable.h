#pragma once

#include "Frost/Renderer/Shader.h"

namespace Frost
{

	// NOTE: Only available for Vulkan/DX12
	class ShaderBindingTable
	{
	public:
		virtual ~ShaderBindingTable() {}

		virtual void Destroy() = 0;

		static Ref<ShaderBindingTable> Create(const Ref<Shader>& shader);
	};

}