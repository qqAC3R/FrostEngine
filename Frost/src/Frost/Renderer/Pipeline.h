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

	enum class DepthCompare
	{
		Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Never, Always
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
			DepthCompare DepthCompareOperation = DepthCompare::Less;
			float LineWidth = 1.0f;
			bool UseDepthTest = true;
			bool UseDepthWrite = true;
			bool UseStencil = false;
			bool Wireframe = false;

			// Optional
			bool ConservativeRasterization = false;
		};

		virtual void Bind() = 0;
		virtual void Unbind() = 0;

		virtual void Destroy() = 0;

		static Ref<Pipeline> Create(Pipeline::CreateInfo& createInfo);
	};

}