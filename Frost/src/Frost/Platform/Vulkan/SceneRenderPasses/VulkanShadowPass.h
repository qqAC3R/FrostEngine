#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Renderer.h"

#define SHADOW_MAP_CASCADE_COUNT 4

namespace Frost
{
	class VulkanShadowPass : public SceneRenderPass
	{
	public:
		VulkanShadowPass();
		virtual ~VulkanShadowPass();

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

		void ShadowDepthInitData();
		void ShadowDepthUpdate(const RenderQueue& renderQueue);

		void ShadowComputeInitData(uint32_t width, uint32_t height);
		void ShadowComputeUpdate(const RenderQueue& renderQueue);

		void UpdateCascades(const RenderQueue& renderQueue);
		void CalculateCascadeOffsets();

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> ShadowDepthShader;
			Ref<Pipeline> ShadowDepthPipeline;
			Ref<RenderPass> ShadowDepthRenderPass;

			Ref<Shader> ShadowComputeShader;
			Ref<ComputePipeline> ShadowComputePipeline;
			Vector<Ref<Material>> ShadowComputeDescriptor;
			Vector<Ref<Image2D>> ShadowComputeTexture;


			Vector<HeapBlock> IndirectVoxelCmdBuffer;

			glm::mat4 CascadeViewProjMatrix[SHADOW_MAP_CASCADE_COUNT];
			float CascadeDepthSplit[SHADOW_MAP_CASCADE_COUNT];

			glm::vec2 MinResCascade[SHADOW_MAP_CASCADE_COUNT];
			glm::vec2 MaxResCascade[SHADOW_MAP_CASCADE_COUNT];
		};

		float CascadeSplitLambda = 0.97f;
		float CameraFarClip = 2000.0f;
		float CameraNearClip = 0.1f;
		float CascadeFarPlaneOffset = 0.0f, CascadeNearPlaneOffset = 0.0f;
		int32_t FadeCascades = 1;
		float CascadesFadeFactor = 10.0f;
		int32_t m_ShowCascadesDebug = 0;
		int32_t UsePCSS = 1;

		struct PushConstantData
		{
			glm::mat4 ViewProjectionMatrix;
			uint64_t VertexBufferBDA;
			uint32_t CascadeIndex;
		};
		PushConstantData m_PushConstant;

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}