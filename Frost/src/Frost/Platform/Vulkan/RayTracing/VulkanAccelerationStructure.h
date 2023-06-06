#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"
#include "Frost/Renderer/RayTracing/AccelerationStructures.h"

#include "Frost/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace Frost
{
	class VulkanBottomLevelAccelerationStructure : public BottomLevelAccelerationStructure
	{
	public:
		VulkanBottomLevelAccelerationStructure(const MeshASInfo& meshInfo);
		virtual ~VulkanBottomLevelAccelerationStructure();

		virtual void Destroy() override;
	private:
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

		Vector<uint32_t> m_GeometryOffset;
		uint32_t m_GeometryMaxOffset;

		VkAccelerationStructureInstanceKHR m_InstanceKHR;
		
		friend class VulkanTopLevelAccelertionStructure;
		friend class VulkanRayTracingPass;
	private:
		GeometryInfo MeshToVkGeometry(const MeshASInfo& meshInfo);
		void BuildBLAS(GeometryInfo geometry);
		void UpdateInstanceInfo();
	};



	class VulkanTopLevelAccelertionStructure : public TopLevelAccelertionStructure
	{
	public:
		VulkanTopLevelAccelertionStructure();
		virtual ~VulkanTopLevelAccelertionStructure();

		virtual void UpdateAccelerationStructure(Vector<std::pair<Ref<MeshAsset>, glm::mat4>>& meshes) override;
		const VkAccelerationStructureKHR& GetVulkanAccelerationStructure() const { return m_AccelerationStructure; }
		VkWriteDescriptorSetAccelerationStructureKHR& GetVulkanDescriptorInfo() { return m_DescriptorInfo; }

		virtual void Destroy() override;
	private:
		VkAccelerationStructureInstanceKHR InstanceToVkGeometryInstance(Ref<BottomLevelAccelerationStructure> blas, const glm::mat4& transform, uint32_t blasIndexID);
		void BuildTLAS(std::vector<VkAccelerationStructureInstanceKHR>& instances);
		void UpdateDescriptor();
	private:
		VkAccelerationStructureKHR m_AccelerationStructure = VK_NULL_HANDLE;
		VkBuffer m_ASBuffer;
		VulkanMemoryInfo m_ASBufferMemory;
		VkWriteDescriptorSetAccelerationStructureKHR m_DescriptorInfo;
		uint32_t m_ASBufferSize;

		Ref<BufferDevice> m_InstanceBuffer;
	};

}