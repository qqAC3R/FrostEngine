#pragma once

#include "Frost/Renderer/SceneRenderPass.h"

#include "Frost/Renderer/Shader.h"
#include "Frost/Renderer/PipelineCompute.h"

#include "Frost/Renderer/Renderer.h"

namespace Frost
{

	class VulkanComputeRenderPass : public SceneRenderPass
	{
	public:
		VulkanComputeRenderPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void OnUpdate(const RenderQueue& renderQueue);
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }
	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;
		std::string m_Name;

		struct InternalData
		{
			Ref<ComputePipeline> ComputePipeline;

			Ref<TextureCubeMap> CubeMap;
			Ref<UniformBuffer> UniformBuffer;
			Ref<Material> Descriptor;
			Ref<Shader> Shader;
		};
		InternalData* m_Data;

		glm::vec3 m_TurbidityAzimuthInclination;

		friend class SceneRenderPassPipeline;
	};


}