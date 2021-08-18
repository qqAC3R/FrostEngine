#pragma once

#include "Frost/Renderer/SceneRenderPass.h"

#include "Frost/Renderer/RayTracing/ShaderBindingTable.h"
#include "Frost/Renderer/RayTracing/RayTracingPipeline.h"

#define FRAMES_IN_FLIGHT 3

namespace Frost
{

	class VulkanRayTracingPass : public SceneRenderPass
	{
	public:
		VulkanRayTracingPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void OnUpdate(const RenderQueue& renderQueue, void* cmdBuf, uint32_t swapChainIndex) override;
		virtual void OnUpdate(const RenderQueue& renderQueue) {} // This function is not needed because it is made for OpenGL (vulkan needs frames in flight)
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
		};
		CameraInfo m_CameraInfo;




		friend class VkRenderer;
		friend class SceneRenderPassPipeline;
	};


}