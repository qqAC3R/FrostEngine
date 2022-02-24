#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

namespace Frost
{
	class VulkanCompositePass : public SceneRenderPass
	{
	public:
		VulkanCompositePass();
		virtual ~VulkanCompositePass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void OnUpdate(const RenderQueue& renderQueue);
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }
	private:
		void TiledLightCulling_DataInit();
		void TiledLightCulling_OnUpdate(const RenderQueue& renderQueue);
		void LightBufferBlur_OnUpdate(const RenderQueue& renderQueue);
	private:
		std::string m_Name;
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			// Composite pass
			Ref<Pipeline> CompositePipeline;
			Ref<RenderPass> RenderPass;
			Vector<Ref<Material>> Descriptor;
			Ref<Shader> CompositeShader;

			// Light data
			Vector<HeapBlock> PointLightBufferData;
			Vector<Ref<BufferDevice>> PointLightIndices;

			// Light culling
			Ref<Shader> LightCullingShader;
			Ref<ComputePipeline> LightCullingPipeline;
			Vector<Ref<Material>> LightCullingDescriptor;
			Vector<Ref<BufferDevice>> CullingDataBuffer;

			// Skybox pass
			Ref<Shader> SkyboxShader;
			Ref<Pipeline> SkyboxPipeline;
			Ref<Material> SkyboxDescriptor;
			Ref<VertexBuffer> SkyBoxVertexBuffer;
		};
		InternalData* m_Data;


		struct PushConstantData
		{
			glm::vec4 CameraPosition;
		};
		PushConstantData m_PushConstantData;

		struct RendererData // For the Tiled LightCulling compute shader
		{
			glm::vec2 ScreenSize;
			int PointLightCount;
			int Padding;

			glm::mat4 ViewMatrix;
			glm::mat4 ProjectionMatrix;
			glm::mat4 ViewProjectionMatrix;
		};
		RendererData m_RendererData;

		friend class SceneRenderPassPipeline;
	};
}