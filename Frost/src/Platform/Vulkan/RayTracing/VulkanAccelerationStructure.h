#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "Frost/Renderer/Mesh.h"

#include "Platform/Vulkan/Buffers/VulkanBufferAllocator.h"
#include "Frost/Renderer/RayTracing/AccelerationStructures.h"


namespace Frost
{




	enum class AccelerationStructureType
	{
		BottomLevel,
		TopLevel
	};


	class AccelerationStructureBuffer
	{
	public:
		AccelerationStructureBuffer();
		virtual ~AccelerationStructureBuffer();

		void CreateAccelerationStructure(size_t size, AccelerationStructureType type);

		static Ref<AccelerationStructureBuffer> Create();

		VkAccelerationStructureKHR GetAccelerationStructure() { return m_AccelerationStructure; }
		virtual VkBuffer GetBuffer() { return m_Buffer; }
		virtual size_t GetBufferSize() { return m_BufferSize; }

		void Detroy();

	private:
		VkAccelerationStructureKHR m_AccelerationStructure;

		VkBuffer m_Buffer;
		VulkanMemoryInfo m_BufferMemory;

		size_t m_BufferSize;
	};







#if 0
	struct VertexMeshLayout
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Color;
		glm::vec2 Texture_Coords;
	};

	struct BlasSpec
	{
		// Data used to build acceleration structure geometry
		std::vector<VkAccelerationStructureGeometryKHR>       Geometry;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR> BuildOffsetInfo;
	};
#endif

	class VulkanBottomLevelAccelerationStructure : public BottomLevelAccelerationStructure
	{
	public:
		VulkanBottomLevelAccelerationStructure(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);
		virtual ~VulkanBottomLevelAccelerationStructure();

		virtual uint32_t GetAccelerationStructureID() const override { return m_AccelerationStructureID; }



		virtual void Destroy() override;

		// Data used to build acceleration structure geometry
		struct GeometryInfo
		{
			Vector<VkAccelerationStructureGeometryKHR> Geometry;
			Vector<VkAccelerationStructureBuildRangeInfoKHR> BuildOffsetInfo;
		};

	private:
		GeometryInfo MeshToVkGeometry(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);
		void BuildBLAS(const GeometryInfo& geometry);

		//void BuildBLAS2(const std::vector<GeometryInfo>& input, VkBuildAccelerationStructureFlagsKHR flags);

	public:


	private:

#if 0
		struct VulkanBlasEntry
		{
			// User-provided input.
			BlasSpec Input;

			// AccelerationStructure and other info needed for memory allocator
			AccelerationStructureBuffer AccelerationStructure;

			// TODO: Not really needed
			VkBuildAccelerationStructureFlagsKHR Flags = 0;

			VulkanBlasEntry() = default;
			VulkanBlasEntry(BlasSpec input_)
				: Input(std::move(input_)), AccelerationStructure()
			{ }
		};

		VulkanBlasEntry m_Blas;
#endif

		GeometryInfo m_GeometryInfo;
		Ref<AccelerationStructureBuffer> m_AccelerationStructure;



		//glm::mat4 m_Transform;
		uint32_t m_AccelerationStructureID;

		friend class VulkanTopLevelAccelertionStructure;
	};



	class VulkanTopLevelAccelertionStructure : public TopLevelAccelertionStructure
	{
	public:
		VulkanTopLevelAccelertionStructure();
		virtual ~VulkanTopLevelAccelertionStructure();

		virtual void UpdateAccelerationStructure(Vector<std::pair<Ref<Mesh>, glm::mat4>>& meshes) override;

		virtual const VkAccelerationStructureKHR& GetVulkanAccelerationStructure() override { return m_VkAccelerationStructure; }

		virtual void Destroy() override;
	private:
		VkAccelerationStructureInstanceKHR InstanceToVkGeometryInstance(std::pair<Ref<Mesh>, glm::mat4>& meshBLAS);
		void BuildTLAS(std::vector<VkAccelerationStructureInstanceKHR>& instances);
		//void BuildTLAS2(std::vector<VkAccelerationStructureInstanceKHR>& instances);

	private:
		VkAccelerationStructureKHR m_VkAccelerationStructure;
		Ref<AccelerationStructureBuffer> m_AccelerationStructure;

		Ref<Buffer> m_InstanceBuffer;

		//VkBuffer m_InstanceBuffer;
		//VulkanMemoryInfo m_InstanceBufferMemory;


	};

}