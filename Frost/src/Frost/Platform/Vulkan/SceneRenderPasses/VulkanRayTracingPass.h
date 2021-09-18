#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Renderer/RayTracing/ShaderBindingTable.h"
#include "Frost/Renderer/RayTracing/RayTracingPipeline.h"

namespace Frost
{

	class VulkanRayTracingPass : public SceneRenderPass
	{
	public:
		VulkanRayTracingPass();
		virtual ~VulkanRayTracingPass();

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
			Ref<TopLevelAccelertionStructure> TopLevelAS[FRAMES_IN_FLIGHT];
			Ref<ShaderBindingTable> SBT;
			Ref<RayTracingPipeline> Pipeline;

			Ref<Buffer> SceneVertexData[FRAMES_IN_FLIGHT];
			Ref<Buffer> SceneIndexData[FRAMES_IN_FLIGHT];
			Ref<Buffer> SceneTransformData[FRAMES_IN_FLIGHT];

			Ref<Image2D> DisplayTexture[FRAMES_IN_FLIGHT];;
			
			Ref<Material> Descriptor[FRAMES_IN_FLIGHT];;
			Ref<Shader> Shader;
		};

		InternalData m_Data;
		std::string m_Name;

		struct CameraInfo
		{
			glm::mat4 InverseView;
			glm::mat4 InverseProjection;
			glm::vec3 CameraPosition;
			int RayAccumulation = 0;
		};
		CameraInfo m_CameraInfo;

		struct InstanceInfo
		{
			glm::mat4 Transform;
			glm::mat4 InverseTransform;

			glm::vec3 Emittance{ 0.0f };
			float Roughness{ 0.0f };
			float RefractionIndex = 1.0f;
		};

		friend class SceneRenderPassPipeline;
	};


}