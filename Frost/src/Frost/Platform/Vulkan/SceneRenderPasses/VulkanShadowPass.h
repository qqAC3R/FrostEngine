#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Renderer.h"

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
		//void ShadowDepthUpdate(const RenderQueue& renderQueue);
		void ShadowDepthUpdateInstancing(const RenderQueue& renderQueue);

		void ShadowComputeInitData(uint32_t width, uint32_t height);
		void ShadowComputeUpdate(const RenderQueue& renderQueue);

		void ShadowComputeDenoiseInitData(uint32_t width, uint32_t height);
		void ShadowComputeDenoiseUpdate(const RenderQueue& renderQueue);

		void UpdateCascades(const RenderQueue& renderQueue);
		void CalculateCascadeOffsets();

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct MeshInstancedVertexBuffer // `MeshInstancedVertexBuffer` For Shadow Pass
		{
			glm::mat4 ModelSpaceMatrix;
			glm::mat4 WorldSpaceMatrix;
			uint64_t BoneInformationBDA;
		};

		struct InternalData
		{
			Ref<Shader> ShadowDepthShader;
			Ref<Pipeline> ShadowDepthPipeline;
			Ref<RenderPass> ShadowDepthRenderPass;

			Ref<Shader> ShadowComputeShader;
			Ref<ComputePipeline> ShadowComputePipeline;
			Vector<Ref<Material>> ShadowComputeDescriptor;
			Vector<Ref<Image2D>> ShadowComputeTexture;

			Ref<Shader> ShadowComputeDenoiseShader;
			Ref<ComputePipeline> ShadowComputeDenoisePipeline;
			Vector<Ref<Material>> ShadowComputeDenoiseDescriptor;
			Vector<Ref<Image2D>> ShadowComputeDenoiseTexture;


			Vector<HeapBlock> IndirectShadowCmdBuffer;

			// Global Instaced Vertex Buffer
			Vector<HeapBlock> GlobalInstancedVertexBuffer;

			glm::mat4 CascadeViewProjMatrix[4];
			glm::vec4 CascadeDepthSplit;

			glm::vec2 MinResCascade[4];
			glm::vec2 MaxResCascade[4];
		};

		struct PushConstantData
		{
			glm::mat4 ViewProjectionMatrix;
			uint64_t VertexBufferBDA;
			uint32_t IsAnimated = 0;
		};
		PushConstantData m_PushConstant;

		struct PushConstantShadowComputeData
		{
			glm::mat4 InvViewProjection;
			glm::vec4 CascadeDepthSplit;
			float NearCameraClip;
			float FarCameraClip;
		};
		PushConstantShadowComputeData m_PushConstantShadowComputeData;

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}