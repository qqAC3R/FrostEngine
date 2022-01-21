#pragma once

#include "Frost/Renderer/Buffers/BufferDevice.h"
#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Core/Buffer.h"

namespace Frost
{

	struct IndirectMeshData
	{
		IndirectMeshData() = default;
		IndirectMeshData(uint32_t submeshOffset, uint32_t SubmeshCount, uint32_t meshIndex)
			: SubmeshOffset(submeshOffset), SubmeshCount(SubmeshCount), MeshIndex(meshIndex)
		{}

		uint32_t SubmeshOffset; // Offset of the last submeshes (used for indirect drawing)
		uint32_t SubmeshCount; // How many submeshes were actually submitted (we are doing culling)
		uint32_t MeshIndex; // The index from the render queue
	};

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
		void Geometry_DataInit();
		void HZB_DataInit();
		void LateCull_DataInit();

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
		};

		struct MeshData
		{
			glm::mat4 Transform;
			glm::vec4 AABB_Min;
			glm::vec4 AABB_Max;
		};

		struct ComputeShaderPS // Push constant
		{
			glm::vec4 DepthPyramidSize; // vec2 DepthPyramidRes || float DrawCount || float Padding
			glm::mat4 ViewProjectionMatrix;
		};

		struct InternalData
		{
			// Geometry pass
			Ref<Pipeline> Pipeline;
			Ref<RenderPass> RenderPass;
			Ref<Shader> GeometryShader;
			Vector<Ref<Material>> Descriptor;


			// Depth pyramid construction
			Ref<Shader> HZBShader; // Hi-Z Buffer Builder Compute Shader
			Ref<ComputePipeline> HZBPipeline;
			Vector<Vector<Ref<Material>>> HZBDescriptor;
			Vector<Ref<Image2D>> DepthPyramid;
			uint32_t HZB_MipLevels;
			glm::vec2 HZB_Dimensions;

			// For occlusion culling
			Ref<Shader> LateCullShader;
			Ref<ComputePipeline> LateCullPipeline;
			Vector<Ref<Material>> LateCullDescriptor;
			
			HeapBlock MeshSpecs; 

			Ref<BufferDevice> DebugDeviceBuffer; // Debug
			ComputeShaderPS ComputeShaderPushConstant; // Push constant data for the occlusion culling shader





			// Indirect drawing
			Vector<HeapBlock> IndirectCmdBuffer;
			Vector<HeapBlock> MaterialSpecs;


		};

		struct PushConstant
		{
			uint32_t MaterialIndex;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}