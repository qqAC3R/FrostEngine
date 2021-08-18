#include "frostpch.h"
#include "AccelerationStructures.h"

#include "Frost/Renderer/RendererAPI.h"
#include "Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

namespace Frost
{



	Ref<BottomLevelAccelerationStructure> BottomLevelAccelerationStructure::Create(const Ref<VertexBuffer>& vertexBuffer,
																				   const Ref<IndexBuffer>& indexBuffer)
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "OpenGL::API doesn't support RayTracing!");
			case RendererAPI::API::Vulkan: return Ref<VulkanBottomLevelAccelerationStructure>::Create(vertexBuffer, indexBuffer);
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<TopLevelAccelertionStructure> TopLevelAccelertionStructure::Create()
	{
		switch (RendererAPI::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::OpenGL: FROST_ASSERT(false, "OpenGL::API doesn't support RayTracing!");
			case RendererAPI::API::Vulkan: return Ref<VulkanTopLevelAccelertionStructure>::Create();
		}

		FROST_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}


#if 0

#include "Platform/Vulkan/VulkanContext.h"

#include "Platform/Vulkan/Buffers/VulkanBufferAllocator.h"


#define NVVK_ALLOC_DEDICATED
#include "nvvk/allocator_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"

//#include "Platform/Vulkan/VulkanCommandBuffer.h"

namespace Frost
{

	//static Scope<VulkanDevice>* m_Device;


	AccelerationStructure::AccelerationStructure()
		//: m_Device(VkApplication::Get().GetVulkanDevicePtr())
	{
		//m_Device = VkApplication::Get().GetVulkanDevicePtr();

		//m_Allocator.init(m_Device->get()->GetDevice(), m_Device->get()->GetPhysicalDevice());

		//vk::PhysicalDevice physicalDevice = m_Device->get()->GetPhysicalDevice();
		//
		//auto properties = physicalDevice.getProperties2KHR<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
		//
		//m_Properties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

		//VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructureProperties = {};
		//accelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
		//
		//VkPhysicalDeviceRayTracingPipelinePropertiesKHR raytracingPipelineProperties = {};
		//raytracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		//raytracingPipelineProperties.pNext = &accelerationStructureProperties;
		//
		//VkPhysicalDeviceProperties2 props = {};
		//props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		//props.pNext = &raytracingPipelineProperties;

		//vkGetPhysicalDeviceProperties2(m_Device->get()->GetPhysicalDevice(), &props);



		//m_Builder.setup(m_Device->get()->GetDevice(), &m_Allocator, m_Device->get()->GetQueues().m_GQIndex);
	}

	AccelerationStructure::~AccelerationStructure()
	{

	}

	void AccelerationStructure::CreateBottomLevelAS(std::vector<Ref<VulkanMesh>>& meshes)
	{
		std::vector<VulkanBlasInput> allBlas;
		allBlas.reserve(meshes.size());

		for (auto& mesh : meshes)
		{
			auto blas = MeshToVkGeometry(mesh);

			allBlas.emplace_back(blas);

			m_BlasSize++;
		}


		BuildBLAS(allBlas);

	}

	void AccelerationStructure::CreateTopLevelAS(std::vector<MeshInstance> instances)
	{
		std::vector<VulkanInstance> tlas;

		tlas.reserve(m_BlasSize);

		for (uint32_t i = 0; i < static_cast<uint32_t>(m_BlasSize); i++)
		{
			//nvvk::RaytracingBuilderKHR::Instance rayInst;
			VulkanInstance rayInst;
			rayInst.transform = instances[i].transform;
			rayInst.instanceCustomId = i;

			rayInst.blasId = i;
			rayInst.hitGroupId = 0;
			rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

			tlas.emplace_back(rayInst);


		}

		//VulkanInstance rayInst;
		//rayInst.transform = glm::translate(glm::mat4(1.0f), glm::vec3(9.0f, 0.0f, 1.0f));
		//rayInst.instanceCustomId = 1;
		//
		//rayInst.blasId = 1;
		//rayInst.hitGroupId = 0;
		//rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		//
		//tlas.emplace_back(rayInst);


		BuildTLAS(tlas);

	}

	//--------------------------------------------------------------------------------------------------
	// Creating the top-level acceleration structure from the vector of Instance
	// - See struct of Instance
	// - The resulting TLAS will be stored in m_tlas
	// - update is to rebuild the Tlas with updated matrices
	void AccelerationStructure::BuildTLAS(const std::vector<VulkanInstance>& instances,
										  VkBuildAccelerationStructureFlagsKHR flags,
										  bool                                 update)
	{
		// Cannot call buildTlas twice except to update.
		//assert(m_tlas.as.accel == VK_NULL_HANDLE || update);


		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t graphicsQueueIndex = VulkanContext::GetCurrentDevice()->GetQueues().m_GQIndex;

		nvvk::CommandPool genCmdBuf(device, graphicsQueueIndex);
		VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();
		//VulkanCommandPool cmdPool;
		//auto cmdBuf = VulkanBufferAllocator::BeginSingleTimeCommands(cmdPool.GetCommandPool());


		m_Tlas.flags = flags;

		// Convert array of our Instances to an array native Vulkan instances.
		std::vector<VkAccelerationStructureInstanceKHR> geometryInstances;
		geometryInstances.reserve(instances.size());
		for (const auto& inst : instances)
		{
			geometryInstances.push_back(InstanceToVkGeometryInstance(inst));
		}

		// Allocate the instance buffer and copy its contents from host to device memory
		//if (update)
		//	m_alloc->destroy(m_instBuffer);

		
		
		//VkBuffer instanceBuffer;
		//VkDeviceMemory instanceDeviceMemory;
		VulkanBufferAllocator::CreateBuffer(geometryInstances.size() * sizeof(VkAccelerationStructureInstanceKHR),
							{ BufferType::ShaderAddress },
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							m_InstanceBuffer, m_InstanceBufferMemory, geometryInstances.data());
		
		


		VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		bufferInfo.buffer = m_InstanceBuffer;

		VkDeviceAddress instanceAddress = vkGetBufferDeviceAddress(device, &bufferInfo);

		// Make sure the copy of the instance buffer are copied before triggering the
		// acceleration structure build
		VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0, 1, &barrier, 0, nullptr, 0, nullptr);


		//--------------------------------------------------------------------------------------------------

		// Create VkAccelerationStructureGeometryInstancesDataKHR
		// This wraps a device pointer to the above uploaded instances.
		VkAccelerationStructureGeometryInstancesDataKHR instancesVk{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
		instancesVk.arrayOfPointers = VK_FALSE;
		instancesVk.data.deviceAddress = instanceAddress;

		// Put the above into a VkAccelerationStructureGeometryKHR. We need to put the
		// instances struct in a union and label it as instance data.
		VkAccelerationStructureGeometryKHR topASGeometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
		topASGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		topASGeometry.geometry.instances = instancesVk;

		// Find sizes
		VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
		buildInfo.flags = flags;
		buildInfo.geometryCount = 1;
		buildInfo.pGeometries = &topASGeometry;
		buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

		uint32_t                                 count = (uint32_t)instances.size();
		VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &count, &sizeInfo);


		// Create TLAS
		if (update == false)
		{
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
			createInfo.size = sizeInfo.accelerationStructureSize;

			m_Tlas.as = AccelerationStructureBuffer::Create();
			m_Tlas.as->CreateAccelerationStructure(sizeInfo.accelerationStructureSize, AccelerationStructureType::TopLevel);


		}

		// Allocate the scratch memory
		VkBuffer scratchBuffer;
		VkDeviceMemory scratchMemoryBuffer;
		VulkanBufferAllocator::CreateBuffer(sizeInfo.buildScratchSize,
							{ BufferType::ShaderAddress, BufferType::AccelerationStructure },
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 scratchBuffer, scratchMemoryBuffer);

		bufferInfo.buffer = scratchBuffer;

		VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &bufferInfo);


		// Update build information
		buildInfo.srcAccelerationStructure = update ? m_Tlas.as->GetAccelerationStructure() : VK_NULL_HANDLE;
		buildInfo.dstAccelerationStructure = m_Tlas.as->GetAccelerationStructure();
		buildInfo.scratchData.deviceAddress = scratchAddress;


		// Build Offsets info: n instances
		VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{ static_cast<uint32_t>(instances.size()), 0, 0, 0 };
		const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

		// Build the TLAS
		vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);

		genCmdBuf.submitAndWait(cmdBuf);  // queueWaitIdle inside.

		VulkanBufferAllocator::DeleteBuffer(scratchBuffer, scratchMemoryBuffer);

	}

	VulkanBlasInput AccelerationStructure::MeshToVkGeometry(Ref<VulkanMesh>& model)
	{
		vk::Device device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		VkBuffer vertexBuffer = (VkBuffer)model->GetVertexBuffer()->get()->GetBuffer();
		VkBuffer indexBuffer = (VkBuffer)model->GetIndexBuffer()->get()->GetBuffer();
		uint32_t vertexBufferCount = model->GetVertexBuffer()->get()->GetBufferSize() / sizeof(uint32_t);
		uint32_t indexBufferCount = model->GetIndexBuffer()->get()->GetBufferSize() / sizeof(uint32_t);


		VkDeviceAddress vertexAddress = device.getBufferAddress({ vertexBuffer });
		VkDeviceAddress indexAddress = device.getBufferAddress({ indexBuffer });
		

		uint32_t maxPrimitiveCount = (uint32_t)(indexBufferCount / 3);

		VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
		triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;

		triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		triangles.vertexData = *(VkDeviceOrHostAddressConstKHR*)&vertexAddress;
		triangles.vertexStride = sizeof(VertexMeshLayout);
		
		triangles.indexType = VK_INDEX_TYPE_UINT32;
		triangles.indexData = *(VkDeviceOrHostAddressConstKHR*)&indexAddress;
		
		triangles.transformData = {};
		triangles.maxVertex = indexBufferCount;

		//vk::AccelerationStructureGeometryTrianglesDataKHR triangles;
		//triangles.setVertexFormat(vk::Format::eR32G32B32Sfloat);
		//triangles.setVertexData(vertexAddress);
		//triangles.setVertexStride(sizeof(VertexMeshLayout));
		//
		//triangles.setIndexType(vk::IndexType::eUint32);
		//triangles.setIndexData(indexAddress);
		//
		//triangles.setTransformData({});
		//triangles.setMaxVertex(vertexBufferCount);


		VkAccelerationStructureGeometryKHR asGeom{};
		asGeom.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeom.geometry.triangles = triangles;

		VkAccelerationStructureBuildRangeInfoKHR offset{};
		offset.firstVertex = 0;
		offset.primitiveCount = maxPrimitiveCount;
		offset.primitiveOffset = 0;
		offset.transformOffset = 0;

		VulkanBlasInput input;
		input.asGeometry.emplace_back(asGeom);
		input.asBuildOffsetInfo.emplace_back(offset);

		return input;

	}

	void AccelerationStructure::BuildBLAS(const std::vector<VulkanBlasInput>& input, VkBuildAccelerationStructureFlagsKHR flags)
	{
		// Cannot call buildBlas twice.
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t graphicsQueueIndex = VulkanContext::GetCurrentDevice()->GetQueues().m_GQIndex;



		// Make our own copy of the user-provided inputs.
		m_Blas = std::vector<VulkanBlasEntry>(input.begin(), input.end());
		uint32_t nbBlas = static_cast<uint32_t>(m_Blas.size());

		// Preparing the build information array for the acceleration build command.
		// This is mostly just a fancy pointer to the user-passed arrays of VkAccelerationStructureGeometryKHR.
		// dstAccelerationStructure will be filled later once we allocated the acceleration structures.
		std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildInfos(nbBlas);
		for (uint32_t idx = 0; idx < nbBlas; idx++)
		{
			buildInfos[idx].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
			buildInfos[idx].flags = flags;
			buildInfos[idx].geometryCount = (uint32_t)m_Blas[idx].input.asGeometry.size();
			buildInfos[idx].pGeometries = m_Blas[idx].input.asGeometry.data();
			buildInfos[idx].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
			buildInfos[idx].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			buildInfos[idx].srcAccelerationStructure = VK_NULL_HANDLE;
		}

		// Finding sizes to create acceleration structures and scratch
		// Keep the largest scratch buffer size, to use only one scratch for all build
		VkDeviceSize              maxScratch{ 0 };          // Largest scratch buffer for our BLAS
		std::vector<VkDeviceSize> originalSizes(nbBlas);  // use for stats

		for (size_t idx = 0; idx < nbBlas; idx++)
		{
			// Query both the size of the finished acceleration structure and the  amount of scratch memory
			// needed (both written to sizeInfo). The `vkGetAccelerationStructureBuildSizesKHR` function
			// computes the worst case memory requirements based on the user-reported max number of
			// primitives. Later, compaction can fix this potential inefficiency.
			std::vector<uint32_t> maxPrimCount(m_Blas[idx].input.asBuildOffsetInfo.size());

			for (auto tt = 0; tt < m_Blas[idx].input.asBuildOffsetInfo.size(); tt++)
				maxPrimCount[tt] = m_Blas[idx].input.asBuildOffsetInfo[tt].primitiveCount;  // Number of primitives/triangles

			VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
			vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
				&buildInfos[idx], maxPrimCount.data(), &sizeInfo);

			// Create acceleration structure object. Not yet bound to memory.
			VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
			createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
			createInfo.size = sizeInfo.accelerationStructureSize;  // Will be used to allocate memory.

			// Actual allocation of buffer and acceleration structure. Note: This relies on createInfo.offset == 0
			// and fills in createInfo.buffer with the buffer allocated to store the BLAS. The underlying
			// vkCreateAccelerationStructureKHR call then consumes the buffer value.
			
			//m_Blas[idx].as = Buffer::CreateAccelerationStructure(m_Device, createInfo);
			m_Blas[idx].as.CreateAccelerationStructure(sizeInfo.accelerationStructureSize, AccelerationStructureType::BottomLevel);
			
			
			//m_debug.setObjectName(m_blas[idx].as.accel, (std::string("Blas" + std::to_string(idx)).c_str()));
			buildInfos[idx].dstAccelerationStructure = m_Blas[idx].as.GetAccelerationStructure();  // Setting the where the build lands

			// Keeping info
			m_Blas[idx].flags = flags;
			maxScratch = std::max(maxScratch, sizeInfo.buildScratchSize);

			// Stats - Original size
			originalSizes[idx] = sizeInfo.accelerationStructureSize;
		}

		// Allocate the scratch buffers holding the temporary data of the
		// acceleration structure builder
		//nvvk::Buffer scratchBuffer = m_alloc->createBuffer(maxScratch, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		VkBuffer scratchBuffer;
		VkDeviceMemory scratchMemoryBuffer;
		VulkanBufferAllocator::CreateBuffer(maxScratch,
											{ BufferType::Storage, BufferType::ShaderAddress},
											 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
											 scratchBuffer, scratchMemoryBuffer);

		VkBufferDeviceAddressInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
		bufferInfo.buffer = scratchBuffer;
		VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(device, &bufferInfo);


		// Is compaction requested?
		bool doCompaction = (flags & VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR)
			== VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR;

		// Allocate a query pool for storing the needed size for every BLAS compaction.
		VkQueryPoolCreateInfo qpci{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
		qpci.queryCount = nbBlas;
		qpci.queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
		VkQueryPool queryPool;
		vkCreateQueryPool(device, &qpci, nullptr, &queryPool);


		// Allocate a command pool for queue of given queue index.
		// To avoid timeout, record and submit one command buffer per AS build.
		nvvk::CommandPool genCmdBuf(device, graphicsQueueIndex);
		std::vector<VkCommandBuffer> allCmdBufs(nbBlas);

		// Building the acceleration structures
		for (uint32_t idx = 0; idx < nbBlas; idx++)
		{
			auto& blas = m_Blas[idx];
			VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();
			allCmdBufs[idx] = cmdBuf;

			// All build are using the same scratch buffer
			buildInfos[idx].scratchData.deviceAddress = scratchAddress;

			// Convert user vector of offsets to vector of pointer-to-offset (required by vk).
			// Recall that this defines which (sub)section of the vertex/index arrays
			// will be built into the BLAS.
			std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBuildOffset(blas.input.asBuildOffsetInfo.size());
			for (size_t infoIdx = 0; infoIdx < blas.input.asBuildOffsetInfo.size(); infoIdx++)
				pBuildOffset[infoIdx] = &blas.input.asBuildOffsetInfo[infoIdx];

			// Building the AS
			vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfos[idx], pBuildOffset.data());

			// Since the scratch buffer is reused across builds, we need a barrier to ensure one build
			// is finished before starting the next one
			VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
			barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
			barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

			// Write compacted size to query number idx.
			if (doCompaction)
			{
				vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1, &blas.as.GetAccelerationStructure(),
					VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, idx);
			}
		}
		genCmdBuf.submitAndWait(allCmdBufs);  // vkQueueWaitIdle behind this call.
		allCmdBufs.clear();

		// Compacting all BLAS
		if (doCompaction)
		{
			VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();

			// Get the size result back
			std::vector<VkDeviceSize> compactSizes(nbBlas);
			vkGetQueryPoolResults(device, queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
				compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);


			// Compacting
			//std::vector<VulkanAccelerationStructure> cleanupAS(nbBlas);  // previous AS to destroy
			std::vector<AccelerationStructureBuffer> cleanupAS;

			uint32_t                    statTotalOriSize{ 0 }, statTotalCompactSize{ 0 };
			for (uint32_t idx = 0; idx < nbBlas; idx++)
			{
				statTotalOriSize += (uint32_t)originalSizes[idx];
				statTotalCompactSize += (uint32_t)compactSizes[idx];

				// Creating a compact version of the AS
				VkAccelerationStructureCreateInfoKHR asCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
				asCreateInfo.size = compactSizes[idx];
				asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
				
				AccelerationStructureBuffer as;
				as.CreateAccelerationStructure(compactSizes[idx], AccelerationStructureType::BottomLevel);

				// Copy the original BLAS to a compact version
				VkCopyAccelerationStructureInfoKHR copyInfo{ VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR };
				copyInfo.src = m_Blas[idx].as.GetAccelerationStructure();
				copyInfo.dst = as.GetAccelerationStructure();
				copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
				vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
				cleanupAS[idx] = m_Blas[idx].as;
				m_Blas[idx].as = as;
			}
			genCmdBuf.submitAndWait(cmdBuf);  // vkQueueWaitIdle within.

			// Destroying the previous version
			for (auto as : cleanupAS)
				as.Delete();
				//Buffer::DeleteBuffer(&m_Device->get()->GetDevice(), as.buffer.buffer, as.buffer.allocation);

		}

		vkDestroyQueryPool(device, queryPool, nullptr);
	
		VulkanBufferAllocator::DeleteBuffer(scratchBuffer, scratchMemoryBuffer);

	}


	//--------------------------------------------------------------------------------------------------
	// Convert an Instance object into a VkAccelerationStructureInstanceKHR

	VkAccelerationStructureInstanceKHR AccelerationStructure::InstanceToVkGeometryInstance(const VulkanInstance& instance)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		VulkanBlasEntry& blas{ m_Blas[instance.blasId] };


		VkAccelerationStructureDeviceAddressInfoKHR addressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
		addressInfo.accelerationStructure = blas.as.GetAccelerationStructure();
		VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);

		VkAccelerationStructureInstanceKHR gInst{};
		// The matrices for the instance transforms are row-major, instead of
		// column-major in the rest of the application
		
		glm::mat4 transp = glm::transpose(instance.transform);
		//glm::mat4 transposeMat = glm::transpose(instance.transform);
		
		// The gInst.transform value only contains 12 values, corresponding to a 4x3
		// matrix, hence saving the last row that is anyway always (0,0,0,1). Since
		// the matrix is row-major, we simply copy the first 12 values of the
		// original 4x4 matrix
		memcpy(&gInst.transform, &transp, sizeof(gInst.transform));

		gInst.instanceCustomIndex = instance.instanceCustomId;
		gInst.mask = instance.mask;
		gInst.instanceShaderBindingTableRecordOffset = instance.hitGroupId;
		gInst.flags = instance.flags;
		gInst.accelerationStructureReference = blasAddress;
		return gInst;
	}

	void AccelerationStructure::Delete()
	{
		
		for (auto& blas : m_Blas)
		{
			blas.as.Delete();

		}

		m_Tlas.as->Delete();


		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		vkDestroyBuffer(device, m_InstanceBuffer, nullptr);
		vkFreeMemory(device, m_InstanceBufferMemory, nullptr);

		m_Blas.clear();
		m_Tlas = {};
	}


}
#endif