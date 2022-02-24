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
		virtual void OnUpdate(const RenderQueue& renderQueue);
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }

	private:
		void SSR_InitData(uint32_t width, uint32_t height);
		void SSR_Update(const RenderQueue& renderQueue);

		void BlurColorBuffer_InitData(uint32_t width, uint32_t height);
		void BlurColorBuffer_Update(const RenderQueue& renderQueue);

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> SSRShader;
			Ref<ComputePipeline> SSRPipeline;
			Vector<Ref<Material>> SSRDescriptor;

			Vector<Ref<Image2D>> FinalImage;

			// For SSCT
			Vector<Ref<Image2D>> BlurredColorBuffer;
			Ref<Shader> BlurShader;
			Ref<ComputePipeline> BlurPipeline;
			Vector<Vector<Ref<Material>>> BlurShaderDescriptor;
			uint32_t ScreenMipLevel;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}