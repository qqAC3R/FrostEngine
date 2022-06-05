#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Renderer.h"

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
		void Voxelization_Init();
		void Voxelization_Update(const RenderQueue& renderQueue);

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> VoxelizationShader;
			Ref<Pipeline> VoxelizationPipeline;
			Ref<RenderPass> VoxelizationRenderPass;
			Vector<Ref<Material>> VoxelizationDescriptor;
			Vector<Ref<Texture3D>> VoxelizationTexture;
			Vector<Ref<BufferDevice>> VoxelizationDebugBuffer;

			Vector<HeapBlock> IndirectVoxelCmdBuffer;
		};

		struct VoxelProjections
		{
			glm::mat4 X;
			glm::mat4 Y;
			glm::mat4 Z;
		} VoxelProj;

		int32_t VoxelVolumeDimensions = 64;

		struct PushConstant
		{
			glm::mat4 ViewMatrix;
			uint32_t MaterialIndex;
			uint64_t VertexBufferBDA;
			int32_t VoxelDimensions = 64;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};

}