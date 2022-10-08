#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Renderer.h"

typedef struct VkImageView_T* VkImageView;

namespace Frost
{

	class VulkanVoxelizationPass : public SceneRenderPass
	{
	public:
		VulkanVoxelizationPass();
		virtual ~VulkanVoxelizationPass();

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
		// ----------------- Voxelization ------------------------
		void VoxelizationInit();
		void VoxelizationUpdateData(const RenderQueue& renderQueue);
		void VoxelizationUpdateRendering(const RenderQueue& renderQueue);
		// -------------------------------------------------------

		// ------------ Voxel texture filtering ------------------
		void VoxelFilterInit();
		void VoxelFilterUpdate(const RenderQueue& renderQueue);
		// -------------------------------------------------------

		// ------------ Voxel Cone Tracing ------------------
		void VoxelConeTracingInit(uint32_t width, uint32_t height);
		void VoxelConeTracingUpdate(const RenderQueue& renderQueue);
		// -------------------------------------------------------

		void ClearBufferInit();

		void UpdateRenderingSettings();

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> VoxelizationShader;
			Ref<Pipeline> VoxelizationPipeline;
			Ref<RenderPass> VoxelizationRenderPass;
			Vector<Ref<Material>> VoxelizationDescriptor;
			Vector<Ref<Texture3D>> VoxelizationTexture;
			Vector<VkImageView> VoxelTexture_R32UI; // This is used for atomic operations (R32UI = Red channel 32 bits unsigned int)

			Ref<Shader> VoxelFilterShader;
			Ref<ComputePipeline> VoxelFilterPipeline;
			Vector<Ref<Material>> VoxelFilterDescriptor;


			Ref<Shader> VoxelConeTracingShader;
			Ref<ComputePipeline> VoxelConeTracingPipeline;
			Vector<Ref<Material>> VoxelConeTracingDescriptor;
			Vector<Ref<Image2D>> VCT_IndirectDiffuseTexture;
			Vector<Ref<Image2D>> VCT_IndirectSpecularTexture;


			Ref<BufferDevice> ClearBuffer;
			Vector<HeapBlock> IndirectVoxelCmdBuffer;


			int32_t m_VoxelGrid; // Renderer::GetRendererConfig().VoxelTextureResolution
			//float m_VoxelSize = 1.0f; 
			glm::vec3 VoxelCameraPosition = { 0.0f, 0.0f, 0.0f }; // Camera position
			float m_VoxelAABB = 0.0f; // Rounded value of 'm_VoxelGrid' to fixed jittering
		};

		struct VoxelProjections
		{
			glm::mat4 X;
			glm::mat4 Y;
			glm::mat4 Z;
		} m_VoxelAABBProjection;

		struct VoxelizationPushConstant
		{
			glm::mat4 ViewMatrix;

			uint32_t MaterialIndex;
			uint64_t VertexBufferBDA;
			int32_t VoxelDimensions;
			int32_t AtomicOperation = 1;
		} m_VoxelizationPushConstant;

		struct VCTPushConstant
		{
			glm::vec3 VoxelSampleOffset;
			float VoxelGrid;

			glm::vec3 CameraPosition;
			float VoxelTextureSize;

			int32_t UseIndirectDiffuse = 0;
			int32_t UseIndirectSpecular = 0;

			float ConeTraceMaxDistance = 100.0f;
			int32_t ConeTraceMaxSteps = 80;
		} m_VCTPushConstant;

		struct VoxelFilterPushConstant
		{
			glm::mat4 CameraViewMatrix;

			glm::vec4 CameraPosition_SampleMipLevel;

			float ProjectionExtents;
			float VoxelScale;
		} m_VoxelFilterPushConstant;

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};

}