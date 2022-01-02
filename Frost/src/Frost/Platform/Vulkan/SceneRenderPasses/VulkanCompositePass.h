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
		std::string m_Name;
		SceneRenderPassPipeline* m_RenderPassPipeline;


		struct InternalData
		{
			// Composite pass
			Ref<Pipeline> CompositePipeline;
			Ref<RenderPass> RenderPass;
			Vector<Ref<Material>> Descriptor;
			Ref<Shader> CompositeShader;
			//Ref<UniformBuffer> m_PointLightUniformBuffer;

			// Light data
			Vector<HeapBlock> PointLightBufferData;

			// For depth pyramid (occlusion culling)
			Vector<Ref<Image2D>> PreviousDepthbuffer;

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

		struct PointLightProperties
		{
			glm::vec3 Position;
			glm::vec3 Radiance;
			float Radius;
			float Falloff;
		};

#if 0
		struct PointLightData
		{
			uint32_t LightCount = 0;
			Vector<PointLightProperties> PointLights;
		};
		PointLightData* m_PointLightData;
#endif

		friend class SceneRenderPassPipeline;
	};
}