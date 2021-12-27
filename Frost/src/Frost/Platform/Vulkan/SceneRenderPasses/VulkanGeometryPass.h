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
			// PBR material values
			glm::vec4 AlbedoColor; // Using a vec4 instead of vec3 because of vulkan's alignment (the offset should be a mutiple of 4)
			float Emission;
			float Roughness;
			float Metalness;
			uint32_t UseNormalMap;
			
			// Texture IDs
			uint32_t AlbedoTextureID;
			uint32_t RoughessTextureID;
			uint32_t MetalnessTextureID;
			uint32_t NormalTextureID;

			// Matricies
			glm::mat4 WorldSpaceMatrix;
			glm::mat4 ModelMatrix;

			// SubMesh indices
			//uint32_t WorldMeshIndex;
			//uint32_t SubmeshIndex;
		};

		struct InternalData
		{
			Ref<Pipeline> Pipeline;
			Ref<RenderPass> RenderPass;
			Ref<Shader> Shader;
			Vector<Ref<Material>> Descriptor;

			//Vector<HeapBlock> VertexBuffer; // Vertex buffer of the scene
			//Vector<HeapBlock> IndexBuffer; // Index buffer of the scene

			Vector<HeapBlock> IndirectCmdBuffer;
			Vector<HeapBlock> InstanceSpecs;

			//Vector<Ref<BufferDevice>> IndirectCmdBuffer;
		};

		struct PushConstant
		{
			uint32_t  MaterialIndex;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}