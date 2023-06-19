#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

typedef struct VkImage_T* VkImage;

namespace Frost
{
	
	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		uint32_t TexIndex;
	};

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	struct TextVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoord;
		uint32_t TexIndex;
	};

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

		uint32_t ReadPixelFromTextureEntityID(uint32_t x, uint32_t y);

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }
	private:

		// ------------------- Batch Renderer --------------------
		void BatchRendererInitData(uint32_t width, uint32_t height);
		void BatchRendererUpdate(const RenderQueue& renderQueue);
		void SubmitBillboard(const RenderQueue& renderQueue, const RenderQueue::Object2D& object2d);
		void SubmitQuad(const RenderQueue::Object2D& object2d); // TODO
		void SubmitText(const RenderQueue::TextObject2D& textObject2D);
		void SubmitLine(const RenderQueue::Object2D& object2d);
		// ------------------------------------------------------

		// ------------------- Render Wireframe --------------------
		void RenderWireframeInitData(uint32_t width, uint32_t height);
		void RenderWireframeUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------

		// ------------------- Render Grid --------------------
		void RenderGridInitData(uint32_t width, uint32_t height);
		void RenderGridUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------

		// ------------ On Click Select Entity ----------------
		void SelectEntityInitData(uint32_t width, uint32_t height);
		void SelectEntityUpdate(const RenderQueue& renderQueue);
		//void SelectEntityUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------

		// ------------ Glow Selected Entity ----------------
		void GlowSelectedEntityInitData(uint32_t width, uint32_t height);
		void GlowSelectedEntityUpdate(const RenderQueue& renderQueue);
		void BlurSelectedEntityUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------


	private:
		std::string m_Name;
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			// This is common for both quads and lines
			Ref<RenderPass> BatchRendererRenderPass;
			Vector<Ref<Material>> BatchRendererMaterial;

			/// Quads renderer
			Ref<Shader> BatchQuadRendererShader;
			Ref<Pipeline> BatchQuadRendererPipeline;

			Vector<Ref<BufferDevice>> QuadVertexBuffer;
			Ref<IndexBuffer> QuadIndexBuffer;
			uint32_t QuadIndexCount = 0;
			uint32_t QuadCount = 0;
			QuadVertex* QuadVertexBufferBase = nullptr;
			QuadVertex* QuadVertexBufferPtr = nullptr;

			/// Lines Renderer
			Ref<Shader> BatchLineRendererShader;
			Ref<Pipeline> BatchLineRendererPipeline;

			Vector<Ref<BufferDevice>> LineVertexBuffer;
			uint32_t LineVertexCount = 0;
			LineVertex* LineVertexBufferBase = nullptr;
			LineVertex* LineVertexBufferPtr = nullptr;

			/// Text Renderer
			Vector<Ref<BufferDevice>> TextVertexBuffer;
			uint32_t TextIndexCount = 0;
			uint32_t TextCount = 0;
			TextVertex* TextVertexBufferBase = nullptr;
			TextVertex* TextVertexBufferPtr = nullptr;



			// Wireframe Renderer
			Ref<Shader> RenderWireframeShader;
			Ref<Pipeline> RenderWireframePipeline;

			// Grid Renderer
			Ref<Shader> RenderGridShader;
			Ref<Pipeline> RenderGridPipeline;
			Ref<BufferDevice> GridVertexBuffer;
			Ref<IndexBuffer> GridIndexBuffer;

			// Used for selecting and entity
			Vector<Ref<Image2D>> EntityIDTexture_CPU;
			uint32_t* TextureDataCPU;
			glm::ivec2 ViewportSize;

			// Line detection
			Ref<Shader> LineDetectionShader;
			Ref<ComputePipeline> LineDetectionPipeline;
			Vector<Ref<Material>> LineDetectionMaterial;
			
			// Glow entity which is selected
			Ref<Shader> GlowSelectedEntityShader;
			Ref<ComputePipeline> GlowSelectedEntityPipeline;
			Vector<Ref<Material>> GlowSelectedEntityMaterial;
			Vector<Ref<Image2D>> GlowSelectedEntityTexturePing;
			Vector<Ref<Image2D>> GlowSelectedEntityTexturePong;


		};
		InternalData* m_Data;

		const glm::vec4 QuadVertexPositions[4] = {
				{ -0.5f, -0.5f, 0.0f, 1.0f },
				{ -0.5f,  0.5f, 0.0f, 1.0f },
				{  0.5f,  0.5f, 0.0f, 1.0f },
				{  0.5f, -0.5f, 0.0f, 1.0f }
		};

		HashMap<VkImage, uint32_t> m_BindlessAllocatedTextures;


		struct BatchQuadRenderPushConstant
		{
			glm::mat4 ViewProjectionMatrix;
			uint32_t UseAtlas;
		};
		BatchQuadRenderPushConstant m_BatchQuadRenderPushConstant;

		struct RenderWireframePushConstant
		{
			glm::mat4 WorldSpaceMatrix;
			glm::vec4 Color;
		};
		RenderWireframePushConstant m_WireframePushConstant;

		struct RenderGridPushConstant
		{
			glm::mat4 WorldSpaceMatrix;
			float GridScale = 16.025f;
			float GridSize = 0.025f;
		};
		RenderGridPushConstant m_GridPushConstant;

		struct GlowSelectedEntityPushConstant
		{
			glm::vec4 SelectedEntityColor = glm::vec4(0.9f, 0.3f, 0.0f, 1.0f);
			uint32_t SelectedEntityID;
			uint32_t FilterMode;
			float CosTime;
		};
		GlowSelectedEntityPushConstant m_GlowSelectedEntityPushConstant;

		friend class SceneRenderPassPipeline;
		friend class VulkanRendererDebugger;
	};
}