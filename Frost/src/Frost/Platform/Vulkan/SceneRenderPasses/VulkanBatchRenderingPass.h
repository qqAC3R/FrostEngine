#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

typedef struct VkImage_T* VkImage;

namespace Frost
{
	

	class VulkanBatchRenderingPass  : public SceneRenderPass
	{
	public:
		VulkanBatchRenderingPass();
		virtual ~VulkanBatchRenderingPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void InitLate() override;
		virtual void OnUpdate(const RenderQueue& renderQueue) override;
		virtual void OnRenderDebug() override;
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void OnResizeLate(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }
	private:

		// ------------------- PBR Deffered --------------------
		void BatchRendererInitData(uint32_t width, uint32_t height);
		void BatchRendererUpdate(const RenderQueue& renderQueue);
		void SubmitBillboard(const RenderQueue& renderQueue, const RenderQueue::Object2D& object2d);
		void SubmitQuad(const RenderQueue::Object2D& object2d); // TODO
		// ------------------------------------------------------

	private:
		std::string m_Name;
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct QuadVertex;

		struct InternalData
		{
			Ref<Shader> BatchRendererShader;
			Ref<Pipeline> BatchRendererPipeline;
			Vector<Ref<Material>> BatchRendererMaterial;
			Ref<RenderPass> BatchRendererRenderPass;

			Vector<Ref<BufferDevice>> QuadVertexBuffer;
			Ref<IndexBuffer> QuadIndexBuffer;

			uint32_t QuadIndexCount = 0;
			uint32_t QuadCount = 0;
			QuadVertex* QuadVertexBufferBase = nullptr;
			QuadVertex* QuadVertexBufferPtr = nullptr;
		};
		InternalData* m_Data;

		static const uint64_t MaxQuads = 200000;
		static const uint64_t MaxVertices = MaxQuads * 4;
		static const uint64_t MaxIndices = MaxQuads * 6;
		const glm::vec4 QuadVertexPositions[4] = {
				{ -0.5f, -0.5f, 0.0f, 1.0f },
				{ -0.5f,  0.5f, 0.0f, 1.0f },
				{  0.5f,  0.5f, 0.0f, 1.0f },
				{  0.5f, -0.5f, 0.0f, 1.0f }
		};

		HashMap<VkImage, uint32_t> m_BindlessAllocatedTextures;

		struct QuadVertex
		{
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TexCoord;
			uint32_t TexIndex;
		};

		friend class SceneRenderPassPipeline;
		friend class VulkanRendererDebugger;
	};
}