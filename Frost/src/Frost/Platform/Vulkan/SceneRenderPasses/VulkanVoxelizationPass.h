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

		float GetVoxelGrid() { return glm::ceil(m_Data->m_VoxelGrid * m_Data->m_VoxelSize); }

	private:
		void VoxelizationInit();
		void VoxelizationUpdate(const RenderQueue& renderQueue);

		void VoxelFilterInit();
		void VoxelFilterUpdate();

		void ClearBufferInit();

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


			Ref<Shader> VoxelVisualizerShader;
			Ref<Pipeline> VoxelVisualizerPipeline;
			Ref<RenderPass> VoxelVisualizerRenderPass;
			Vector<Ref<Material>> VoxelVisualizerDescriptor;

			Ref<BufferDevice> ClearBuffer;

			Vector<HeapBlock> IndirectVoxelCmdBuffer;

			int32_t m_VoxelGrid = 256;
			float m_VoxelSize = 1.0f;
			glm::vec3 CameraPosition = { 0.0f, 0.0f, 0.0f };
		};

		struct VoxelProjections
		{
			glm::mat4 X;
			glm::mat4 Y;
			glm::mat4 Z;
		} VoxelProj;

		struct PushConstant
		{
			glm::mat4 ViewMatrix;
			uint32_t MaterialIndex;
			uint64_t VertexBufferBDA;
			int32_t VoxelDimensions = 256;
			int32_t AtomicOperation;
		};
		int32_t m_AtomicOperation = 1;

		int32_t m_Frame[3] = { 0, 0, 0 };
		int32_t m_PingPongVoxelTexture = 0;

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};

}