#pragma once

#include "Frost/Renderer/Buffers/BufferLayout.h"

#include "Frost/Renderer/Shader.h"
#include "Frost/Renderer/RenderPass.h"

namespace Frost
{
	class Material;

	enum class PrimitiveTopology
	{
		None,
		Points,
		Lines,
		LineStrip,
		Triangles,
		TriangleStrip
	};

	enum class CullMode
	{
		None,
		Front,
		Back,
		FrontAndBack
	};

	class Pipeline
	{
	public:
		virtual ~Pipeline() {}

		struct CreateInfo
		{
			Ref<Shader> Shader;
			BufferLayout VertexBufferLayout;
			Ref<RenderPass> RenderPass;

			PrimitiveTopology Topology = PrimitiveTopology::Triangles;
			CullMode Cull = CullMode::None;
			float LineWidth = 1.0f;
			bool UseDepth = true;
			bool UseStencil = false;
		};

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Destroy() = 0;

		static Ref<Pipeline> Create(Pipeline::CreateInfo& createInfo);
	};

}