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
		void PBRInitData(uint32_t width, uint32_t height);
		void PBRUpdate();
		// ------------------------------------------------------

		// --------------- Tiled Light Culling ------------------
		void TiledLightCullingInitData(uint32_t width, uint32_t height);
		void TiledLightCullingUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------

		

		void OnEnvMapChangeCallback(const Ref<TextureCubeMap>& prefiltered, const Ref<TextureCubeMap>& irradiance);
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
			Vector<Ref<BufferDevice>> PointLightIndicesVolumetric;

			// Light culling
			Ref<Shader> LightCullingShader;
			Ref<ComputePipeline> LightCullingPipeline;
			Vector<Ref<Material>> LightCullingDescriptor;
		};
		InternalData* m_Data;


		struct PushConstantData
		{
			glm::vec4 CameraPosition;
			glm::vec3 DirectionalLightDir;
			int32_t UseLightHeatMap = 0;
		};
		PushConstantData m_PushConstantData;

		struct TiledLightCullPushConstant // For the Tiled LightCulling compute shader
		{
			glm::mat4 ViewMatrix;
			glm::mat4 ProjectionMatrix;
			glm::mat4 ViewProjectionMatrix;

			glm::vec2 ScreenSize;
			int PointLightCount;

		};
		TiledLightCullPushConstant m_TiledLIghtCullPushConstant;


		friend class SceneRenderPassPipeline;
		friend class VulkanRendererDebugger;
	};
}