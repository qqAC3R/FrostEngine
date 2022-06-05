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
		IndirectMeshData(uint32_t submeshOffset, uint32_t SubmeshCount, uint32_t meshIndex, uint32_t materialCount, uint32_t materialOffset)
			: SubmeshOffset(submeshOffset), SubmeshCount(SubmeshCount), MeshIndex(meshIndex), MaterialCount(materialCount), MaterialOffset(materialOffset)
		{}

		uint32_t SubmeshOffset; // Offset of the last submeshes (used for indirect drawing)
		uint32_t SubmeshCount; // How many submeshes were actually submitted (we are doing culling)
		uint32_t MeshIndex; // The index from the render queue

		uint32_t MaterialCount;
		uint32_t MaterialOffset;
	};

	class VulkanGeometryPass : public SceneRenderPass
	{
	public:
		VulkanGeometryPass();
		virtual ~VulkanGeometryPass();

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
		void Geometry_DataInit();
		void LateCull_DataInit(uint32_t width, uint32_t height);

		void OcclusionCullUpdate(const RenderQueue& renderQueue, uint64_t indirectCmdsOffset);

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct MaterialData
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
		};

		struct MeshData_OC // Data for occlusion culling
		{
			glm::mat4 Transform;
			glm::vec4 AABB_Min;
			glm::vec4 AABB_Max;
		};

		struct ComputeShaderPS // Push constant
		{
			glm::vec4 DepthPyramidSize;
			glm::mat4 ViewMatrix;
			glm::mat4 ProjectionMaxtrix;
			float CamNear;
			float CamFar;
			float Padding1;
			float Padding2;
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
			glm::mat4 ViewMatrix;
			uint32_t MaterialIndex;
			uint64_t VertexBufferBDA;
			uint32_t Padding_ = 0;
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}