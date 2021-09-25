#include "frostpch.h"
#include "VulkanAccelerationStructure.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"

#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanIndexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBuffer.h"

namespace Frost
{

	static uint32_t s_BlasIDs = 0;

	VulkanBottomLevelAccelerationStructure::VulkanBottomLevelAccelerationStructure(const Ref<VertexBuffer>& vertexBuffer,
																				   const Ref<IndexBuffer>& indexBuffer)
	{
		m_GeometryInfo = MeshToVkGeometry(vertexBuffer, indexBuffer);

		BuildBLAS(m_GeometryInfo);

		m_AccelerationStructureID = s_BlasIDs;
		s_BlasIDs++;
	}

	VulkanBottomLevelAccelerationStructure::~VulkanBottomLevelAccelerationStructure()
	{
	}

	VulkanBottomLevelAccelerationStructure::GeometryInfo VulkanBottomLevelAccelerationStructure::MeshToVkGeometry(const Ref<VertexBuffer>& vertexBuffer,																													const Ref<IndexBuffer>& indexBuffer)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		VkDeviceAddress vertexAddressBuffer, indexAddressBuffer;
		{
			// Getting the buffer address
			VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };


			bufferInfo.buffer = vertexBuffer.As<VulkanVertexBuffer>()->GetVulkanBuffer();
			vertexAddressBuffer = vkGetBufferDeviceAddress(device, &bufferInfo);

			bufferInfo.buffer = indexBuffer.As<VulkanIndexBuffer>()->GetVulkanBuffer();
			indexAddressBuffer = vkGetBufferDeviceAddress(device, &bufferInfo);

		}

		uint32_t maxPrimitiveCount = indexBuffer->GetCount();

		VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
		trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		trianglesData.vertexData = *(VkDeviceOrHostAddressConstKHR*)&vertexAddressBuffer;
		trianglesData.vertexStride = sizeof(Vertex);
		trianglesData.indexType = VK_INDEX_TYPE_UINT32;
		trianglesData.indexData = *(VkDeviceOrHostAddressConstKHR*)&indexAddressBuffer;
		trianglesData.transformData = VkDeviceOrHostAddressConstKHR();
		trianglesData.maxVertex = vertexBuffer->GetBufferSize() / sizeof(Vertex);

		VkAccelerationStructureGeometryKHR ASGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		ASGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		ASGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		ASGeometry.geometry.triangles = trianglesData;

		VkAccelerationStructureBuildRangeInfoKHR offset{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		offset.firstVertex = 0;
		offset.transformOffset = 0;
		offset.primitiveCount = maxPrimitiveCount;
		offset.primitiveOffset = 0;

		VulkanBottomLevelAccelerationStructure::GeometryInfo input;
		input.Geometry.emplace_back(ASGeometry);
		input.BuildOffsetInfo.emplace_back(offset);

		return input;
	}

	void VulkanBottomLevelAccelerationStructure::BuildBLAS(GeometryInfo geomtryInfo)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

		VkAccelerationStructureBuildGeometryInfoKHR buildInfos{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfos.flags = flags;
		buildInfos.geometryCount = (uint32_t)geomtryInfo.Geometry.size();
		buildInfos.pGeometries = geomtryInfo.Geometry.data();
		buildInfos.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfos.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		buildInfos.srcAccelerationStructure = m_AccelerationStructure ? m_AccelerationStructure : VK_NULL_HANDLE;

		// Query both the size of the finished acceleration structure and the  amount of scratch memory
		// needed (both written to sizeInfo). The `vkGetAccelerationStructureBuildSizesKHR` function
		// computes the worst case memory requirements based on the user-reported max number of
		// primitives. Later, compaction can fix this potential inefficiency.
		Vector<uint32_t> maxPrimCount(geomtryInfo.BuildOffsetInfo.size());
		for (auto i = 0; i < geomtryInfo.BuildOffsetInfo.size(); i++)
			maxPrimCount[i] = geomtryInfo.BuildOffsetInfo[i].primitiveCount;

		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfos, maxPrimCount.data(), &sizeInfo);



		{
			// Build the acceleration structure
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			m_ASBufferSize = (uint32_t)sizeInfo.accelerationStructureSize;

			Vector<BufferType> usages;
			usages.push_back(BufferType::AccelerationStructure);
			usages.push_back(BufferType::ShaderAddress);

			VulkanAllocator::AllocateBuffer(m_ASBufferSize, usages, MemoryUsage::GPU_ONLY, m_ASBuffer, m_ASBufferMemory);

			VkAccelerationStructureCreateInfoKHR asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			asInfo.size = m_ASBufferSize;
			asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			asInfo.buffer = m_ASBuffer;

			vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &m_AccelerationStructure);

			VulkanContext::SetStructDebugName("BottomLevel-AccelerationStructure", VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, m_AccelerationStructure);
			VulkanContext::SetStructDebugName("BottomLevel-AccelerationStructure-Buffer", VK_OBJECT_TYPE_BUFFER, m_ASBuffer);
			buildInfos.dstAccelerationStructure = m_AccelerationStructure;
		}


		VkDeviceSize scratchBufferSize = sizeInfo.buildScratchSize;

		// Allocate the scratch buffers holding the temporary data of the acceleration structure builder
		VkBuffer scratchBuffer;
		VulkanMemoryInfo scratchMemoryBuffer;
		VulkanAllocator::AllocateBuffer(scratchBufferSize,
			{ BufferType::Storage, BufferType::ShaderAddress },
			MemoryUsage::GPU_ONLY,
			scratchBuffer, scratchMemoryBuffer);


		// Is compaction requested?
		bool doCompaction = (flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR) == VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

		// Allocate a query pool for storing the needed size for every BLAS compaction.
		VkQueryPoolCreateInfo qpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		qpci.queryCount = 1;
		qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		VkQueryPool queryPool;
		vkCreateQueryPool(device, &qpci, nullptr, &queryPool);
		vkResetQueryPool(device, queryPool, 0, 1);



		{

			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

			// Convert user vector of offsets to vector of pointer-to-offset (required by vk).
			// This defines which (sub)section of the vertex/index arrays will be built into the BLAS.
			Vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBuildOffset(geomtryInfo.BuildOffsetInfo.size());
			for (size_t i = 0; i < geomtryInfo.BuildOffsetInfo.size(); i++)
				pBuildOffset[i] = &geomtryInfo.BuildOffsetInfo[i];

			VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
			bufferInfo.buffer = scratchBuffer;
			VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &bufferInfo);

			buildInfos.scratchData.deviceAddress = scratchAddress;
			vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfos, pBuildOffset.data());


			VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmdBuf,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				0, 1, &barrier,
				0, nullptr,
				0, nullptr
			);

			// Write compacted size to query number idx.
			if (doCompaction)
			{
				vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &m_AccelerationStructure,
					VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);
			}

			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
	}



		// Compacting all BLAS
		if (doCompaction)
		{
			VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);

			// Get the size result back
			Vector<VkDeviceSize> compactSize(1);
			vkGetQueryPoolResults(device, queryPool, 0, compactSize.size(), compactSize.size() * sizeof(VkDeviceSize), compactSize.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);


			// Compacting
			VkAccelerationStructureKHR cleanUpAS;
			VkBuffer cleanUpASBuffer;
			VulkanMemoryInfo cleanUpASBufferMemory;

			{
				// Creating a compact version of the AS
				VkAccelerationStructureKHR compactedAS;
				VkBuffer compactedASBuffer;
				VulkanMemoryInfo compactedASBufferMemory;

				Vector<BufferType> usages = { BufferType::AccelerationStructure, BufferType::ShaderAddress };
				VulkanAllocator::AllocateBuffer(compactSize[0], usages, MemoryUsage::GPU_ONLY, compactedASBuffer, compactedASBufferMemory);

				VkAccelerationStructureCreateInfoKHR asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
				asInfo.size = compactSize[0];
				asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				asInfo.buffer = m_ASBuffer;
				vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &compactedAS);


				// Copy the original BLAS to a compact version
				VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
				copyInfo.src = m_AccelerationStructure;
				copyInfo.dst = compactedAS;
				copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
				vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);

				cleanUpAS = m_AccelerationStructure;
				cleanUpASBuffer = m_ASBuffer;
				cleanUpASBufferMemory = m_ASBufferMemory;

				m_AccelerationStructure = compactedAS;
				m_ASBuffer = compactedASBuffer;
				m_ASBufferMemory = compactedASBufferMemory;
			}
			VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);



			// Destroying the previous version
			vkDestroyAccelerationStructureKHR(device, cleanUpAS, nullptr);
			VulkanAllocator::DeleteBuffer(cleanUpASBuffer, cleanUpASBufferMemory);

			VulkanContext::SetStructDebugName("BottomLevel-AccelerationStructure", VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, m_AccelerationStructure);
			VulkanContext::SetStructDebugName("BottomLevel-AccelerationStructure-Buffer", VK_OBJECT_TYPE_BUFFER, m_ASBuffer);
		}


		VulkanAllocator::DeleteBuffer(scratchBuffer, scratchMemoryBuffer);
		vkDestroyQueryPool(device, queryPool, nullptr);
	}

	void VulkanBottomLevelAccelerationStructure::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyAccelerationStructureKHR(device, m_AccelerationStructure, nullptr);
		VulkanAllocator::DeleteBuffer(m_ASBuffer, m_ASBufferMemory);
	}












	VulkanTopLevelAccelertionStructure::VulkanTopLevelAccelertionStructure()
	{
	}

	VulkanTopLevelAccelertionStructure::~VulkanTopLevelAccelertionStructure()
	{
	}

	void VulkanTopLevelAccelertionStructure::UpdateAccelerationStructure(Vector<std::pair<Ref<Mesh>, glm::mat4>>& meshes)
	{
		Vector<VkAccelerationStructureInstanceKHR> accelerationStructureInstances;
		for(auto mesh : meshes)
			accelerationStructureInstances.emplace_back(InstanceToVkGeometryInstance(mesh));

		BuildTLAS(accelerationStructureInstances);
		UpdateDescriptor();
	}

	void VulkanTopLevelAccelertionStructure::Destroy()
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		vkDestroyAccelerationStructureKHR(device, m_AccelerationStructure, nullptr);
		VulkanAllocator::DeleteBuffer(m_ASBuffer, m_ASBufferMemory);
		m_InstanceBuffer->Destroy();
	}

	VkAccelerationStructureInstanceKHR VulkanTopLevelAccelertionStructure::InstanceToVkGeometryInstance(std::pair<Ref<Mesh>, glm::mat4>& meshBLAS)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		// Getting the device address of the mesh blas
		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		auto blas = meshBLAS.first->GetAccelerationStructure();
		auto vkBlas = blas.As<VulkanBottomLevelAccelerationStructure>();
		addressInfo.accelerationStructure = vkBlas->m_AccelerationStructure;

		VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);


		// Setting up the instance
		VkAccelerationStructureInstanceKHR accelerationStructureInstanceInfo{};
		glm::mat4 transp = glm::transpose(meshBLAS.second);
		memcpy(&accelerationStructureInstanceInfo.transform, &transp, sizeof(accelerationStructureInstanceInfo.transform));

		accelerationStructureInstanceInfo.instanceCustomIndex = vkBlas->m_AccelerationStructureID;
		accelerationStructureInstanceInfo.mask = 0xFF; // Visibility mask, will be AND-ed with ray mask
		accelerationStructureInstanceInfo.instanceShaderBindingTableRecordOffset = 0; // Idk
		accelerationStructureInstanceInfo.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // Probably will be always this
		accelerationStructureInstanceInfo.accelerationStructureReference = blasAddress;

		return accelerationStructureInstanceInfo;
	}

	void VulkanTopLevelAccelertionStructure::BuildTLAS(std::vector<VkAccelerationStructureInstanceKHR>& instances)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t graphicsQueueIndex = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Index;
		auto flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

		
		VkCommandBuffer cmdBuf = VulkanContext::GetCurrentDevice()->AllocateCommandBuffer(true);
		
		if (!m_AccelerationStructure)
		{
			m_InstanceBuffer = Buffer::Create(1000 * sizeof(VkAccelerationStructureInstanceKHR),
											 { BufferType::ShaderAddress, BufferType::AccelerationStructureReadOnly } );
		}


		m_InstanceBuffer->SetData((uint32_t)instances.size() * sizeof(VkAccelerationStructureInstanceKHR), instances.data());


		// Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);





		// Getting the device address of the created buffer (m_InstanceBuffer) with the mesh instances wrapped in VkAccelerationStructureInstanceKHR
		VkBufferDeviceAddressInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferInfo.buffer = m_InstanceBuffer.As<VulkanBuffer>()->GetVulkanBuffer();
		VkDeviceAddress instanceAddress = vkGetBufferDeviceAddress(device, &bufferInfo);


		//--------------------------------------------------------------------------------------------------

		// Create VkAccelerationStructureGeometryInstancesDataKHR (this wraps a device pointer to the above uploaded instances)
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{};
		instancesVk.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		instancesVk.arrayOfPointers = VK_FALSE;
		instancesVk.data.deviceAddress = instanceAddress;

		// Put the above into a VkAccelerationStructureGeometryKHR (we need to put the instances struct in a union and label it as instance data)
		VkAccelerationStructureGeometryKHR topASGeometry{};
		topASGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topASGeometry.geometry.instances = instancesVk;

		// Find sizes
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topASGeometry;
		buildInfo.mode = m_AccelerationStructure ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		uint32_t count = (uint32_t)instances.size();
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &count, &sizeInfo);


		// Create TLAS
		bool updateTlas = true;
		if (!m_AccelerationStructure)
		{
			// Build the acceleration structure
			
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
			updateTlas = false;
			m_ASBufferSize = (uint32_t)sizeInfo.accelerationStructureSize;

			Vector<BufferType> usages;
			usages.push_back(BufferType::AccelerationStructure);
			usages.push_back(BufferType::ShaderAddress);

			VulkanAllocator::AllocateBuffer(m_ASBufferSize, usages, MemoryUsage::GPU_ONLY, m_ASBuffer, m_ASBufferMemory);

			VkAccelerationStructureCreateInfoKHR asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			asInfo.size = m_ASBufferSize;
			asInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			asInfo.buffer = m_ASBuffer;

			vkCreateAccelerationStructureKHR(device, &asInfo, nullptr, &m_AccelerationStructure);

			VulkanContext::SetStructDebugName("TopLevel-AccelerationStructure", VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, m_AccelerationStructure);
			VulkanContext::SetStructDebugName("TopLevel-AccelerationStructure-Buffer", VK_OBJECT_TYPE_BUFFER, m_ASBuffer);
		}

		// Allocate the scratch memory
		VkBuffer scratchBuffer;
		VulkanMemoryInfo scratchMemoryBuffer;
		VulkanAllocator::AllocateBuffer(sizeInfo.buildScratchSize,
											{ BufferType::ShaderAddress, BufferType::AccelerationStructure },
											MemoryUsage::GPU_ONLY,
											scratchBuffer, scratchMemoryBuffer);
		
		// Getting the device address of the scratch buffer
		VkBufferDeviceAddressInfo scratchBufferInfo{};
		scratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		scratchBufferInfo.buffer = scratchBuffer;
		VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &scratchBufferInfo);




		// Update build information
		buildInfo.srcAccelerationStructure = updateTlas ? m_AccelerationStructure : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = m_AccelerationStructure;
		buildInfo.scratchData.deviceAddress = scratchAddress;


		// Build the TLAS (with n instances)
		VkAccelerationStructureBuildRangeInfoKHR buildOffsetInfo{};
		buildOffsetInfo.primitiveCount = static_cast<uint32_t>(instances.size());
		buildOffsetInfo.primitiveOffset = 0;
		buildOffsetInfo.firstVertex = 0;
		buildOffsetInfo.transformOffset = 0;
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);




		VulkanContext::GetCurrentDevice()->FlushCommandBuffer(cmdBuf);
		VulkanAllocator::DeleteBuffer(scratchBuffer, scratchMemoryBuffer);
	}

	void VulkanTopLevelAccelertionStructure::UpdateDescriptor()
	{
		m_DescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		m_DescriptorInfo.pNext = nullptr;
		m_DescriptorInfo.accelerationStructureCount = 1;
		m_DescriptorInfo.pAccelerationStructures = &m_AccelerationStructure;
	}

}
