#include "frostpch.h"
#include "VulkanRayTracingPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanIndexBuffer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanRayTracingPipeline.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanShaderBindingTable.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanComputePass.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Math/Math.h"

namespace Frost
{
	static glm::vec3 PreviousCameraPosition[3];

	VulkanRayTracingPass::VulkanRayTracingPass()
		: m_Name("RayTracingPass")
	{
	}

	VulkanRayTracingPass::~VulkanRayTracingPass()
	{
	}

	static bool s_UpdateTLAS[3];
	void VulkanRayTracingPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->Shader = Renderer::GetShaderLibrary()->Get("PathTracer");

		ImageSpecification imageSpec{};
		imageSpec.Width = 1600;
		imageSpec.Height = 900;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Format = ImageFormat::RGBA16F;

		auto cubeMapTexture = m_RenderPassPipeline->GetRenderPassData<VulkanComputePass>()->CubeMap;
		uint32_t maxInstances = Renderer::GetRendererConfig().RayTracing.MaxInstance;
		uint32_t maxMeshes = Renderer::GetRendererConfig().MaxMesh;


		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			//  Tlas
			m_Data->TopLevelAS[i] = TopLevelAccelertionStructure::Create();

			// Scene data
			m_Data->SceneVertexData[i] = BufferDevice::Create(sizeof(uint64_t) * maxInstances,
															 { BufferUsage::Storage, BufferUsage::AccelerationStructureReadOnly });

			m_Data->SceneIndexData[i] = BufferDevice::Create(sizeof(uint64_t) * maxInstances,
															 { BufferUsage::Storage, BufferUsage::AccelerationStructureReadOnly });

			m_Data->SceneTransformData[i] = BufferDevice::Create(sizeof(InstanceInfo) * maxInstances,
																{ BufferUsage::Storage, BufferUsage::AccelerationStructureReadOnly });



			// Scene geometry offsets
			m_Data->SceneGeometryOffsets[i].DeviceBuffer = BufferDevice::Create(
				sizeof(uint32_t) * maxMeshes, { BufferUsage::Storage, BufferUsage::AccelerationStructureReadOnly }
			);
			m_Data->SceneGeometryOffsets[i].HostBuffer.Allocate(sizeof(uint32_t) * maxMeshes);
			m_Data->SceneGeometryOffsets[i].HostBuffer.Initialize();

			// Scene geometry submesh count
			m_Data->SceneGeometrySubmeshCount[i].DeviceBuffer = BufferDevice::Create(
				sizeof(uint32_t) * maxMeshes, { BufferUsage::Storage, BufferUsage::AccelerationStructureReadOnly }
			);
			m_Data->SceneGeometrySubmeshCount[i].HostBuffer.Allocate(sizeof(uint32_t) * maxMeshes);
			m_Data->SceneGeometrySubmeshCount[i].HostBuffer.Initialize();


			// Final image
			m_Data->DisplayTexture[i] = Image2D::Create(imageSpec);

			// Descriptor
			m_Data->Descriptor[i] = Material::Create(m_Data->Shader, "RayTracingDescriptor");

			// Main output image
			m_Data->Descriptor[i]->Set("u_Image", m_Data->DisplayTexture[i]);

			// Cubemap
			m_Data->Descriptor[i]->Set("u_CubeMapSky", cubeMapTexture);

			// Scene data
			m_Data->Descriptor[i]->Set("VertexPointers", m_Data->SceneVertexData[i]);
			m_Data->Descriptor[i]->Set("IndexPointers", m_Data->SceneIndexData[i]);
			m_Data->Descriptor[i]->Set("TransformInstancePointers", m_Data->SceneTransformData[i]);

			// Geometry additional information
			m_Data->Descriptor[i]->Set("GeometrySubmeshOffsets", m_Data->SceneGeometryOffsets[i].DeviceBuffer);
			m_Data->Descriptor[i]->Set("GeometrySubmeshCount", m_Data->SceneGeometrySubmeshCount[i].DeviceBuffer);

			auto vulkanMaterial = m_Data->Descriptor[i].As<VulkanMaterial>();
		}


		m_Data->SBT = ShaderBindingTable::Create(m_Data->Shader);


		RayTracingPipeline::CreateInfo createInfo{};
		createInfo.ShaderBindingTable = m_Data->SBT;
		createInfo.Shader = m_Data->Shader;
		m_Data->Pipeline = RayTracingPipeline::Create(createInfo);


		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			s_UpdateTLAS[i] = true;
	}

	void VulkanRayTracingPass::InitLate()
	{
	}

	void VulkanRayTracingPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;

		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		Vector<std::pair<Ref<Mesh>, glm::mat4>> meshes;
		Vector<uint64_t> vertexBufferPointers;
		Vector<uint64_t> indexBufferPointers;
		Vector<InstanceInfo> transformBufferPointers;


		// How the below code works:
		//=====================================================================
		// MESHES:                ||    CAR    || TREE	||       HOUSE       || (example of meshes)
		// SUBMESH_INDEX_OFFSET:  || 0, 24, 56 || 0, 45 || 0, 128, 256, 341	 ||
		//=====================================================================
		// SUBMESH_COUNT:         ||     3     ||   2   ||        4	         ||
		// SUBMESH_COUNT_OFFSET:  ||     0     ||   3   ||        5	         ||
		//=====================================================================
		uint32_t subMeshCount_offset = 0;
		uint32_t subMeshOffsets_offset = 0;
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			auto mesh = renderQueue.m_Data[i];
			bool isCulled = false;
			if (!isCulled)
			{
				meshes.push_back(std::make_pair(mesh.Mesh, mesh.Transform));
				vertexBufferPointers.push_back(mesh.Mesh->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress());
				indexBufferPointers.push_back(mesh.Mesh->GetSubmeshIndexBuffer().As<VulkanIndexBuffer>()->GetVulkanBufferAddress());

				Ref<VulkanBottomLevelAccelerationStructure> blas = mesh.Mesh->GetAccelerationStructure().As<VulkanBottomLevelAccelerationStructure>();

				// Write into the scene buffers
				uint32_t submeshOffsetBufferSize = blas->m_GeometryOffset.size() * sizeof(uint32_t);
				m_Data->SceneGeometryOffsets[currentFrameIndex].HostBuffer.Write(blas->m_GeometryOffset.data(), submeshOffsetBufferSize, subMeshOffsets_offset);

				// Add up the offsets from the mesh data
				subMeshOffsets_offset += submeshOffsetBufferSize;


				if (subMeshCount_offset == 0)
				{
					uint32_t submeshCount = 0;
					m_Data->SceneGeometrySubmeshCount[currentFrameIndex].HostBuffer.Write(&submeshCount, (uint32_t)sizeof(uint32_t), subMeshCount_offset);

					subMeshCount_offset += sizeof(uint32_t);
				}
				else
				{
					auto oldMesh = renderQueue.m_Data[i - 1];
					Ref<VulkanBottomLevelAccelerationStructure> oldBlas = oldMesh.Mesh->GetAccelerationStructure().As<VulkanBottomLevelAccelerationStructure>();

					// Getting the last submesh count
					uint32_t lastSubmeshCount = m_Data->SceneGeometrySubmeshCount[currentFrameIndex].HostBuffer.Read<uint32_t>(subMeshCount_offset - sizeof(uint32_t));

					// Calculating the next offset
					uint32_t submeshCount = oldBlas->m_GeometryMaxOffset + lastSubmeshCount;

					// Setting the buffer with the `submeshCount` data
					m_Data->SceneGeometrySubmeshCount[currentFrameIndex].HostBuffer.Write(&submeshCount, (uint32_t)sizeof(uint32_t), subMeshCount_offset);

					subMeshCount_offset += sizeof(uint32_t);
				}
				
				// TODO: This should use materials
				InstanceInfo instanceInfo{};
				instanceInfo.Transform = mesh.Transform;
				instanceInfo.InverseTransform = glm::inverse(mesh.Transform);
				instanceInfo.Albedo = mesh.Mesh->GetMaterial().ambient;
				instanceInfo.Emittance = mesh.Mesh->GetMaterial().emission;
				instanceInfo.Roughness = mesh.Mesh->GetMaterial().roughness;
				instanceInfo.RefractionIndex = mesh.Mesh->GetMaterial().ior;

				transformBufferPointers.push_back(instanceInfo);
			}
		}
		if (meshes.size() != 0)
			m_Data->TopLevelAS[currentFrameIndex]->UpdateAccelerationStructure(meshes);

		// Scene data
		m_Data->SceneVertexData[currentFrameIndex]->SetData(
			(uint32_t)vertexBufferPointers.size() * sizeof(uint64_t), vertexBufferPointers.data()
		);

		m_Data->SceneIndexData[currentFrameIndex]->SetData(
			(uint32_t)indexBufferPointers.size() * sizeof(uint64_t), indexBufferPointers.data()
		);

		m_Data->SceneTransformData[currentFrameIndex]->SetData(
			(uint32_t)transformBufferPointers.size() * sizeof(InstanceInfo), transformBufferPointers.data()
		);

		// Geometry offsets
		auto bufferGeometryOffsets = m_Data->SceneGeometryOffsets[currentFrameIndex].HostBuffer;
		m_Data->SceneGeometryOffsets[currentFrameIndex].DeviceBuffer->SetData(subMeshOffsets_offset, bufferGeometryOffsets.Data);

		auto bufferGeometrySubmeshCount = m_Data->SceneGeometrySubmeshCount[currentFrameIndex].HostBuffer;
		m_Data->SceneGeometrySubmeshCount[currentFrameIndex].DeviceBuffer->SetData(subMeshCount_offset, bufferGeometrySubmeshCount.Data);

		// Camera info
		m_CameraInfo.InverseProjection = glm::inverse(renderQueue.CameraProjectionMatrix);
		m_CameraInfo.InverseView = glm::inverse(renderQueue.CameraViewMatrix);
		m_CameraInfo.RayAccumulation++;
		m_CameraInfo.CameraPosition = renderQueue.CameraPosition;
		m_CameraInfo.InverseProjection[1][1] *= -1; // Inverting the y coordonate because glm was designed for OpenGL


		if (PreviousCameraPosition[currentFrameIndex] != renderQueue.CameraPosition)
		{
			m_CameraInfo.RayAccumulation = -1;
			PreviousCameraPosition[currentFrameIndex] = renderQueue.CameraPosition;
		}

		// TODO: This should be made better
		if (s_UpdateTLAS[currentFrameIndex])
		{
			m_Data->Descriptor[currentFrameIndex]->Set("u_TopLevelAS", m_Data->TopLevelAS[currentFrameIndex]);
			m_Data->Descriptor[currentFrameIndex].As<VulkanMaterial>()->UpdateVulkanDescriptorIfNeeded();
			s_UpdateTLAS[currentFrameIndex] = false;
		}

		Ref<VulkanRayTracingPipeline> rtPipeline = m_Data->Pipeline.As<VulkanRayTracingPipeline>();
		rtPipeline->Bind();
		rtPipeline->BindVulkanPushConstant("ps_Camera", &m_CameraInfo);

		m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);

		Ref<VulkanShaderBindingTable> rtSbt = m_Data->SBT.As<VulkanShaderBindingTable>();
		auto strideAddresses = rtSbt->GetVulkanShaderAddresses();

		vkCmdTraceRaysKHR(cmdBuf,
			&strideAddresses[0],
			&strideAddresses[1],
			&strideAddresses[2],
			&strideAddresses[3],
			renderQueue.ViewPortWidth, renderQueue.ViewPortHeight, 1
		);

		// Clear the data
		bufferGeometryOffsets.Initialize();
		bufferGeometrySubmeshCount.Initialize();
	}

	void VulkanRayTracingPass::OnRenderDebug()
	{
	}

	void VulkanRayTracingPass::OnResize(uint32_t width, uint32_t height)
	{
		ImageSpecification imageSpec{};
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Format = ImageFormat::RGBA16F;

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data->DisplayTexture[i] = Image2D::Create(imageSpec);
			m_Data->Descriptor[i]->Set("u_Image", m_Data->DisplayTexture[i]);

			auto vulkanMaterial = m_Data->Descriptor[i].As<VulkanMaterial>();
			vulkanMaterial->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanRayTracingPass::OnResizeLate(uint32_t width, uint32_t height)
	{
	}

	void VulkanRayTracingPass::ShutDown()
	{
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data->SceneGeometryOffsets[i].HostBuffer.Release();
			m_Data->SceneGeometrySubmeshCount[i].HostBuffer.Release();
		}

		delete m_Data;
	}

}