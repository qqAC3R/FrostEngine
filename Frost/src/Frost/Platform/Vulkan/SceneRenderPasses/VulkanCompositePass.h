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

		// --------------- Tiled Point Light Culling ------------------
		void TiledPointLightCullingInitData(uint32_t width, uint32_t height);
		void TiledPointLightCullingUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------

		// --------------- Tiled Rectangular Light Culling ------------------
		void TiledRectLightCullingInitData(uint32_t width, uint32_t height);
		void TiledRectLightCullingUpdate(const RenderQueue& renderQueue);
		// ------------------------------------------------------
		

		void OnEnvMapChangeCallback(const Ref<TextureCubeMap>& prefiltered, const Ref<TextureCubeMap>& irradiance);
	private:
		std::string m_Name;
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			// Composite pass
			//Ref<Pipeline> CompositePipeline;
			Ref<RenderPass> RenderPass;

			//Ref<Shader> CompositeShader;
			Ref<Shader> CompositeComputeShader;
			Vector<Ref<Material>> CompositeDescriptor;
			Ref<ComputePipeline> CompositePipeline;
			//Vector<Ref<Image2D>> CompositeImage;


			// Point Light data
			Vector<Ref<BufferDevice>> PointLightBufferData;
			Vector<Ref<BufferDevice>> PointLightIndices;
			Vector<Ref<BufferDevice>> PointLightIndicesVolumetric;
			// Point Light culling
			Ref<Shader> PointLightCullingShader;
			Ref<ComputePipeline> PointLightCullingPipeline;
			Vector<Ref<Material>> PointLightCullingDescriptor;

			// Rectangular Light data
			Vector<Ref<BufferDevice>> RectLightBufferData;
			Vector<Ref<BufferDevice>> RectLightIndices;
			Vector<Ref<BufferDevice>> RectLightIndicesVolumetric;
			// Rectangular Light culling
			Ref<Shader> RectLightCullingShader;
			Ref<ComputePipeline> RectLightCullingPipeline;
			Vector<Ref<Material>> RectLightCullingDescriptor;

			Ref<Texture2D> LTC1_Lut;
			Ref<Texture2D> LTC2_Lut;
		};
		InternalData* m_Data;


		struct PushConstantData
		{
			glm::mat4 InvViewProjMatrix;
			glm::vec4 CameraPosition;
			glm::vec3 DirectionalLightDir;
			float DirectionalLightIntensity;
			int32_t UseLightHeatMap = 0;
		};
		PushConstantData m_PushConstantData;

		struct TiledLightCullPushConstant // For the Tiled LightCulling compute shader
		{
			glm::mat4 ViewMatrix;
			glm::mat4 ProjectionMatrix;
			//glm::mat4 ViewProjectionMatrix;

			//glm::vec2 ScreenSize;
			//int NumberOfLights;
		};
		TiledLightCullPushConstant m_TiledLightCullPushConstant;


		friend class SceneRenderPassPipeline;
		friend class VulkanRendererDebugger;
	};
}