#pragma once

#include <glm/glm.hpp>

using VkAccelerationStructureKHR = struct VkAccelerationStructureKHR_T*;

namespace Frost
{
	class VertexBuffer;
	class IndexBuffer;
	class Mesh;
	
	// Only works for Vulkan/DX12 APIs
	class BottomLevelAccelerationStructure
	{
	public:
		virtual ~BottomLevelAccelerationStructure() {}

		virtual void Destroy() = 0;

		virtual uint32_t GetAccelerationStructureID() const = 0;

		static Ref<BottomLevelAccelerationStructure> Create(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);

	};

	// Only works for Vulkan/DX12 APIs
	class TopLevelAccelertionStructure
	{
	public:
		virtual ~TopLevelAccelertionStructure() {}

		virtual void Destroy() = 0;

		virtual void UpdateAccelerationStructure(Vector<std::pair<Ref<Mesh>, glm::mat4>>& meshes) = 0;


		/* Vulkan specific API*/
		virtual const VkAccelerationStructureKHR& GetVulkanAccelerationStructure() = 0;


		static Ref<TopLevelAccelertionStructure> Create();

	};
	


}

#if 0
#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Buffers/ASBuffer.h"

#include <glm/glm.hpp>

namespace Frost
{

	struct VertexMeshLayout
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Color;
		glm::vec2 Texture_Coords;
	};
	
	
	struct VulkanBlasInput
	{
		// Data used to build acceleration structure geometry
		std::vector<VkAccelerationStructureGeometryKHR>       asGeometry;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> asBuildOffsetInfo;
	};

	// This is an instance of a BLAS
	struct VulkanInstance
	{
		uint32_t blasId{ 0 };            // Index of the BLAS in m_blas
		uint32_t instanceCustomId{ 0 };  // Instance Index (gl_InstanceCustomIndexEXT)
		uint32_t hitGroupId{ 0 };        // Hit group index in the SBT
		uint32_t mask{ 0xFF };           // Visibility mask, will be AND-ed with ray mask
		VkGeometryInstanceFlagsKHR flags{ VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR };
		glm::mat4 transform{ glm::mat4(1.0f) };  // Identity
	};

	struct TLAS
	{
		Ref<AccelerationStructureBuffer> as;
		VkBuildAccelerationStructureFlagsKHR flags = 0;
	};


	class AccelerationStructure
	{
	public:
		AccelerationStructure();
		virtual ~AccelerationStructure();

		void CreateBottomLevelAS(std::vector<Ref<VulkanMesh>>& meshes);
		void CreateTopLevelAS(std::vector<MeshInstance> instances);

		Ref<AccelerationStructureBuffer> GetTLAS() { return m_Tlas.as; }

		void Delete();

	private:


		VulkanBlasInput MeshToVkGeometry(Ref<VulkanMesh>& model);
		
		void BuildBLAS(const std::vector<VulkanBlasInput>& input,
			VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
		
		void BuildTLAS(const std::vector<VulkanInstance>& instances,
			VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, bool update = false);

		VkAccelerationStructureInstanceKHR InstanceToVkGeometryInstance(const VulkanInstance& instance);

	private:
		
		struct VulkanBlasEntry
		{
			// User-provided input.
			VulkanBlasInput input;

			// VkAccelerationStructureKHR plus extra info needed for our memory allocator.
			// The RaytracingBuilderKHR that created this DOES destroy it when destroyed.
			//VulkanAccelerationStructure as;
			AccelerationStructureBuffer as;

			// Additional parameters for acceleration structure builds
			VkBuildAccelerationStructureFlagsKHR flags = 0;

			VulkanBlasEntry() = default;
			VulkanBlasEntry(VulkanBlasInput input_)
				: input(std::move(input_))
				, as()
			{ }
		};


		size_t m_BlasSize = 0;

		std::vector<VulkanBlasEntry> m_Blas;
		TLAS m_Tlas;

		
		VkBuffer m_InstanceBuffer;
		VkDeviceMemory m_InstanceBufferMemory;


	};

}
#endif