#pragma once

#include "Frost/Renderer/Buffers/BufferDevice.h"
#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Core/Buffer.h"

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

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InstanceData
		{
			glm::vec3 AlbedoColor;
			float Emission;
			float Roughness;
			float Metalness;
			uint32_t UseNormalMap;
		};

		struct InternalData
		{
			Ref<Pipeline> Pipeline;
			Ref<RenderPass> RenderPass;
			Ref<Shader> Shader;
			Vector<Ref<Material>> Descriptor;

			Vector<HeapBlock> InstanceData;

			Vector<HeapBlock> VertexBuffer; // Vertex buffer of the scene
			Vector<HeapBlock> IndexBuffer; // Index buffer of the scene

			Vector<Ref<BufferDevice>> IndirectCmdBuffer;
		};

		struct PushConstant
		{
			glm::mat4 TransformMatrix;
			glm::mat4 ModelMatrix;
			uint32_t  MaterialIndex;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}