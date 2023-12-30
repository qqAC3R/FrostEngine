#pragma once

#include "Frost/Renderer/Buffers/BufferDevice.h"
#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Core/Buffer.h"

typedef struct VkQueryPool_T* VkQueryPool;

namespace Frost
{

#if 0
	struct IndirectMeshData
	{
		IndirectMeshData() = default;
		IndirectMeshData(uint32_t submeshOffset, uint32_t SubmeshCount, uint32_t meshIndex, uint32_t materialCount, uint32_t materialOffset)
			: SubmeshOffset(submeshOffset), SubmeshCount(SubmeshCount), MeshIndex(meshIndex), MaterialCount(materialCount), MaterialOffset(materialOffset)
		{}

		IndirectMeshData(uint32_t submeshOffset, uint32_t SubmeshCount, uint32_t meshIndex, uint32_t materialCount, uint32_t materialOffset, AssetHandle meshAssetHandle)
			: SubmeshOffset(submeshOffset), SubmeshCount(SubmeshCount), MeshIndex(meshIndex), MaterialCount(materialCount), MaterialOffset(materialOffset), MeshAssetHandle(meshAssetHandle)
		{}

		uint32_t SubmeshOffset; // Offset of the last submeshes (used for indirect drawing)
		uint32_t SubmeshCount; // How many submeshes were actually submitted (we are doing culling)
		uint32_t MeshIndex; // The index from the render queue

		uint32_t MaterialCount;
		uint32_t MaterialOffset;

		AssetHandle MeshAssetHandle;
	};
#endif

	struct NewIndirectMeshData
	{
		NewIndirectMeshData() = default;

		uint64_t CmdOffset;          /// DONE

		uint64_t TotalMeshOffset;    /// DONE // Same as `TotalSubmeshCount` however with the added Offset from the previous mesh
		uint64_t SubmeshCount;       /// DONE   // Reponsible for Submesh Count (without instances) and for the Indirect Command Numbers
		uint64_t InstanceCount;      /// DONE
		uint64_t TotalSubmeshCount;  /// DONE // SubmeshCount * InstanceCount

		uint64_t MaterialCount;	     /// DONE
		uint64_t MaterialOffset;     /// DONE

		Vector<uint64_t> SubMeshIndexCulled;

		AssetHandle MeshAssetHandle; /// DONE
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
		// -------------------- Geometry Pass ------------------------
		void GeometryDataInit(uint32_t width, uint32_t height);
		//void GeometryPrepareIndirectData(const RenderQueue& renderQueue);
		void GeometryPrepareIndirectDataWithInstacing(const RenderQueue& renderQueue);
		//void GeometryUpdate(const RenderQueue& renderQueue, const Vector<IndirectMeshData>& indirectMeshData);
		void GeometryUpdateWithInstancing(const RenderQueue& renderQueue);
		// -----------------------------------------------------------

		// ------------------- Occlusion Culling ---------------------
		void OcclusionCullDataInit(uint32_t width, uint32_t height);
		void OcclusionCullUpdate(const RenderQueue& renderQueue, uint64_t indirectCmdsOffset);
		void GetOcclusionQueryResults();
		// -----------------------------------------------------------

		//void ObjectCullingPrepareData(const RenderQueue& renderQueue);

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

#if 0
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
#endif

		struct MeshInstancedVertexBuffer // `MeshInstancedVertexBuffer` For Geometry Pass
		{
			glm::mat4 ModelSpaceMatrix;
			glm::mat4 WorldSpaceMatrix;
			glm::mat4 PreviousWorldSpaceMatrix;
			uint64_t BoneInformationBDA;
			uint32_t MaterialIndexOffset;
			uint32_t EntityID;
			//uint32_t IsMeshVisible;
			//uint32_t a_Padding0;
			//uint32_t a_Padding1;
			//uint32_t a_Padding2;
		};

		struct InternalData
		{
			// Geometry pass
			Ref<Pipeline> GeometryPipeline;
			Ref<RenderPass> GeometryRenderPass;
			Ref<Shader> GeometryShader;
			Vector<Ref<Material>> GeometryDescriptor;


			// For occlusion culling
			Ref<Shader> LateCullShader;
			Ref<ComputePipeline> LateCullPipeline;
			Vector<Ref<Material>> LateCullDescriptor;
			
			Vector<HeapBlock> MeshSpecs; 

			//Ref<BufferDevice> DebugDeviceBuffer; // Debug
			//ComputeShaderPS ComputeShaderPushConstant; // Push constant data for the occlusion culling shader


			// Indirect drawing
			Vector<HeapBlock> IndirectCmdBuffer;
			Vector<HeapBlock> MaterialSpecs;

			// Global Instaced Vertex Buffer
			Vector<HeapBlock> GlobalInstancedVertexBuffer;


			Vector<VkQueryPool> OcclusionQueryPools;
		};

		struct PushConstant
		{
			glm::mat4 ViewMatrix;
			glm::vec2 JitterCurrent;
			glm::vec2 JitterPrevious;
			uint64_t VertexBufferBDA;
			uint32_t IsAnimated = 0;
		} m_GeometryPushConstant;

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}