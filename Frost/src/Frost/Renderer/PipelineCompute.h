#pragma once

#include "Frost/Renderer/Shader.h"

namespace Frost
{
	class ComputePipeline
	{
	public:
		virtual ~ComputePipeline() {}

		struct CreateInfo
		{
			Ref<Shader> Shader;
		};

		virtual void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) = 0;

		virtual void Destroy() = 0;

		static Ref<ComputePipeline> Create(const ComputePipeline::CreateInfo& createInfo);
	};
}