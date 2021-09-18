#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

namespace Frost
{

	class VulkanGeometryPass : public SceneRenderPass
	{
	public:
		VulkanGeometryPass();
		virtual ~VulkanGeometryPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void OnUpdate(const RenderQueue& renderQueue);
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return &m_Data; }

		virtual const std::string& GetName() override { return m_Name; }

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Pipeline> Pipeline;
			Ref<RenderPass> RenderPass;
			Ref<Framebuffer> Framebuffer[FRAMES_IN_FLIGHT];
			Ref<Material> Descriptor[FRAMES_IN_FLIGHT];;
			Ref<Shader> Shader;
		};

		InternalData m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};


}