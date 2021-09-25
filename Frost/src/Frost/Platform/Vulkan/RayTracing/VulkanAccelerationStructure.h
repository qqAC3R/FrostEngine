#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"
#include <glm/glm.hpp>

#include "Frost/Renderer/Mesh.h"

#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"
#include "Frost/Renderer/RayTracing/AccelerationStructures.h"


namespace Frost
{

	class VulkanBottomLevelAccelerationStructure : public BottomLevelAccelerationStructure
	{
	public:
		VulkanBottomLevelAccelerationStructure(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);
		virtual ~VulkanBottomLevelAccelerationStructure();

		virtual void Destroy() override;

	private:
		uint32_t m_AccelerationStructureID;

		VkAccelerationStructureKHR m_AccelerationStructure = VK_NULL_HANDLE;
		VkBuffer m_ASBuffer;
		VulkanMemoryInfo m_ASBufferMemory;
		uint32_t m_ASBufferSize;


		// Data used to build acceleration structure geometry
		struct GeometryInfo
		{
			Vector<VkAccelerationStructureGeometryKHR> Geometry;
			Vector<VkAccelerationStructureBuildRangeInfoKHR> BuildOffsetInfo;
		};
		GeometryInfo m_GeometryInfo;
		friend class VulkanTopLevelAccelertionStructure;

	private:
		GeometryInfo MeshToVkGeometry(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);
		void BuildBLAS(GeometryInfo geometry);
	};



	class VulkanTopLevelAccelertionStructure : public TopLevelAccelertionStructure
	{
	public:
		VulkanTopLevelAccelertionStructure();
		virtual ~VulkanTopLevelAccelertionStructure();

		virtual void UpdateAccelerationStructure(Vector<std::pair<Ref<Mesh>, glm::mat4>>& meshes) override;
		const VkAccelerationStructureKHR& GetVulkanAccelerationStructure() const { return m_AccelerationStructure; }
		VkWriteDescriptorSetAccelerationStructureKHR& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }

		virtual void Destroy() override;
	private:
		VkAccelerationStructureInstanceKHR InstanceToVkGeometryInstance(std::pair<Ref<Mesh>, glm::mat4>& meshBLAS);
		void BuildTLAS(std::vector<VkAccelerationStructureInstanceKHR>& instances);
		void UpdateDescriptor();
	private:
		VkAccelerationStructureKHR m_AccelerationStructure = VK_NULL_HANDLE;
		VkBuffer m_ASBuffer;
		VulkanMemoryInfo m_ASBufferMemory;
		VkWriteDescriptorSetAccelerationStructureKHR m_DescriptorInfo;
		uint32_t m_ASBufferSize;

		Ref<Buffer> m_InstanceBuffer;
	};

}