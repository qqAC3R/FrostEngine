#pragma once

#include "ShaderBindingTable.h"
#include "Frost/Renderer/Pipeline.h"

namespace Frost
{

	enum class ShaderType;
	class Material;

	// Only works for Vulkan/DX12 APIs
	class RayTracingPipeline
	{
	public:
		struct CreateInfo
		{
			Ref<Shader> Shader;
			Ref<ShaderBindingTable> ShaderBindingTable;
		};

		virtual ~RayTracingPipeline() {}

		virtual void Destroy() = 0;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		static Ref<RayTracingPipeline> Create(const RayTracingPipeline::CreateInfo& pipelineCreateInfo);

	};


}