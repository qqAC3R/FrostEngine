#pragma once

#include "Frost/Renderer/SceneRenderPass.h"

namespace Frost
{
	class VulkanPostFXPass : public SceneRenderPass
	{
	public:
		VulkanPostFXPass();
		virtual ~VulkanPostFXPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void InitLate() override;
		virtual void OnUpdate(const RenderQueue& renderQueue);
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void OnResizeLate(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }

	private:
		void SSR_InitData(uint32_t width, uint32_t height);
		void SSR_Update(const RenderQueue& renderQueue);

		void BlurColorBuffer_InitData(uint32_t width, uint32_t height);
		void BlurColorBuffer_Update(const RenderQueue& renderQueue);

		void HZB_InitData(uint32_t width, uint32_t height);
		void HZB_Update(const RenderQueue& renderQueue);

		void Visibility_InitData(uint32_t width, uint32_t height);
		void Visibility_Update(const RenderQueue& renderQueue);

		void AO_InitData(uint32_t width, uint32_t height);
		void AO_Update(const RenderQueue& renderQueue);

		void SpatialDenoiser_InitData(uint32_t width, uint32_t height);
		void SpatialDenoiser_Update(const RenderQueue& renderQueue);

		void CalculateMipLevels(uint32_t width, uint32_t height);

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			// General
			uint32_t ScreenMipLevel;
			Vector<Ref<Image2D>> FinalImage;

			// SSR
			Ref<Shader> SSRShader;
			Ref<ComputePipeline> SSRPipeline;
			Vector<Ref<Material>> SSRDescriptor;

			// Pre-filtered color buffer
			Vector<Ref<Image2D>> BlurredColorBuffer;
			Ref<Shader> BlurShader;
			Ref<ComputePipeline> BlurPipeline;
			Vector<Vector<Ref<Material>>> BlurShaderDescriptor;

			// Depth pyramid construction
			Ref<Shader> HZBShader; // Hi-Z Buffer Builder Compute Shader
			Ref<ComputePipeline> HZBPipeline;
			Vector<Vector<Ref<Material>>> HZBDescriptor;
			Vector<Ref<Image2D>> DepthPyramid;

			// Visibility buffer
			Ref<Shader> VisibilityShader;
			Ref<ComputePipeline> VisibilityPipeline;
			Vector<Vector<Ref<Material>>> VisibilityDescriptor;
			Vector<Ref<Image2D>> VisibilityImage;

			struct VisBuffer_PushConstant
			{
				uint32_t CurrentMipLevel;
				float NearPlane;
				float FarPlane;
			} m_VisBufferPushConstant;

			// GTAO pass
			Ref<Shader> AO_Shader;
			Ref<ComputePipeline> AO_Pipeline;
			Vector<Ref<Material>> AO_Descriptor;
			Vector<Ref<Image2D>> AO_Image;
			
			struct AO_PushConstant
			{
				glm::mat4 ViewMatrix;
				glm::mat4 InvProjMatrix;

				// vec4:                                       x        ||     y,z   
				glm::vec3 AO_Data = glm::vec3(0.0f); // ProjectionScale || ScreenSize
				int32_t AO_Mode = 0;

			} m_AO_PushConstant;

			// Denoising pass
			Ref<Shader> DenoiserShader;
			Ref<ComputePipeline> DenoiserPipeline;
			Vector<Ref<Material>> DenoiserDescriptor;
			Vector<Ref<Image2D>> DenoiserImage;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}