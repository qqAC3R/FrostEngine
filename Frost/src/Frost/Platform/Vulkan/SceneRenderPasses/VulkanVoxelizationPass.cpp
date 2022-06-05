#include "frostpch.h"
#include "VulkanVoxelizationPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"

namespace Frost
{
	VulkanVoxelizationPass::VulkanVoxelizationPass()
		: m_Name("VoxelizationPass")
	{
	}

	VulkanVoxelizationPass::~VulkanVoxelizationPass()
	{
	}

	void VulkanVoxelizationPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->VoxelizationShader = Renderer::GetShaderLibrary()->Get("Voxelization");

		Voxelization_Init();
	}

	void VulkanVoxelizationPass::Voxelization_Init()
	{
		// Getting all the needed information
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().GeometryPass_Mesh_Count;

		// RenderPass
		RenderPassSpecification renderPassSpecs =
		{
			(uint32_t)VoxelVolumeDimensions, (uint32_t)VoxelVolumeDimensions, 3,
			{
				{
					FramebufferTextureFormat::R8, ImageUsage::Storage,
					OperationLoad::DontCare, OperationStore::DontCare, // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},
			}
		};
		m_Data->VoxelizationRenderPass = RenderPass::Create(renderPassSpecs);


		// Pipeline
		BufferLayout bufferLayout = {
			{ "a_ModelSpaceMatrix",  ShaderDataType::Mat4 },
			{ "a_WorldSpaceMatrix",  ShaderDataType::Mat4 },
		};
		bufferLayout.m_InputType = InputType::Instanced;

		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->VoxelizationShader;
		pipelineCreateInfo.UseDepthTest = false;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.UseStencil = false;
		pipelineCreateInfo.Cull = CullMode::None;
		pipelineCreateInfo.RenderPass = m_Data->VoxelizationRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data->VoxelizationPipeline = Pipeline::Create(pipelineCreateInfo);

		m_Data->VoxelizationTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = VoxelVolumeDimensions;
			imageSpec.Height = VoxelVolumeDimensions;
			imageSpec.Depth = VoxelVolumeDimensions;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.UseMipChain = false;

			m_Data->VoxelizationTexture[i] = Texture3D::Create(imageSpec);
		}

		m_Data->VoxelizationDebugBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->VoxelizationDebugBuffer[i] = BufferDevice::Create(32 * 32 * sizeof(glm::vec3), { BufferUsage::Storage });
		}

		m_Data->VoxelizationDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->VoxelizationDescriptor[i] = Material::Create(m_Data->VoxelizationShader, "Voxelization_Material");
			
			auto& instanceSpec = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->MaterialSpecs[i];

			Ref<VulkanMaterial> descriptor = m_Data->VoxelizationDescriptor[i].As<VulkanMaterial>();

			descriptor->Set("u_VoxelTexture", m_Data->VoxelizationTexture[i]);
			descriptor->Set("DebugBuffer", m_Data->VoxelizationDebugBuffer[i]);
			descriptor->Set("u_MaterialUniform", instanceSpec.DeviceBuffer);
			descriptor->UpdateVulkanDescriptorIfNeeded();
		}

		m_Data->IndirectVoxelCmdBuffer.resize(framesInFlight);
		for (auto& indirectCmdBuffer : m_Data->IndirectVoxelCmdBuffer)
		{
			// Allocating a heap block
			indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes, { BufferUsage::Storage, BufferUsage::Indirect });
			indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes);
		}
	}

	void VulkanVoxelizationPass::Voxelization_Update(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();

		glm::vec3 camPos = renderQueue.CameraPosition;
		
		float camPosX = camPos.x;
		float camPosY = camPos.y;
		float camPosZ = camPos.z;

		//FROST_CORE_INFO("X: {0}   Y: {1}   Z: {2}", camPosX, camPosY, camPosZ);

		float size = VoxelVolumeDimensions;

		// X
		glm::mat4 projectionMatrix = glm::ortho(
			-size * 0.5f, size * 0.5f,
			-size * 0.5f, size * 0.5f,
			 size * 0.5f, size * 1.5f
		);

		VoxelProj.X = projectionMatrix * glm::lookAt(glm::vec3(size, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
		VoxelProj.Y = projectionMatrix * glm::lookAt(glm::vec3(0, size, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
		VoxelProj.Z = projectionMatrix * glm::lookAt(glm::vec3(0, 0, size), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		VoxelProj.X[1][1] *= -1.0f;
		VoxelProj.Y[1][1] *= -1.0f;
		VoxelProj.Z[1][1] *= -1.0f;

		m_Data->VoxelizationDescriptor[currentFrameIndex]->Set("VoxelProjections.AxisX", VoxelProj.X);
		m_Data->VoxelizationDescriptor[currentFrameIndex]->Set("VoxelProjections.AxisY", VoxelProj.Y);
		m_Data->VoxelizationDescriptor[currentFrameIndex]->Set("VoxelProjections.AxisZ", VoxelProj.Z);

	}

	void VulkanVoxelizationPass::InitLate()
	{

	}

	void VulkanVoxelizationPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;

		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->VoxelizationRenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->VoxelizationPipeline.As<VulkanPipeline>();

		// Get the needed matricies from the camera
		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectionMatrix * renderQueue.CameraViewMatrix;




		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
		*/
		Vector<IndirectMeshData> meshIndirectData;

		// `Indirect draw commands` offset
		uint64_t indirectCmdsOffset = 0;

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetSubMeshes();

			// Count how many meshes were submitted (for calculating offsets)
			uint32_t submittedSubmeshes = 0;

			// Set commands for the submeshes
			for (uint32_t k = 0; k < submeshes.size(); k++)
			{
				const Submesh& submesh = submeshes[k];

				glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;

				// Submit the submesh into the cpu buffer
				VkDrawIndexedIndirectCommand indirectCmdBuf{};
				indirectCmdBuf.firstIndex = submesh.BaseIndex;
				indirectCmdBuf.firstInstance = 0;
				indirectCmdBuf.indexCount = submesh.IndexCount;
				indirectCmdBuf.instanceCount = 1;
				indirectCmdBuf.vertexOffset = submesh.BaseVertex;
				m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);


				// Adding up the offset
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
				submittedSubmeshes += 1;
			}

			// If we are submitting the first mesh, we don't need any offset
			if (meshIndirectData.size() == 0)
			{
				meshIndirectData.emplace_back(IndirectMeshData(0, submeshes.size(), i, mesh->GetMaterialCount(), 0));
			}
			else
			{
				uint32_t previousMeshOffset = meshIndirectData[i - 1].SubmeshOffset;
				uint32_t previousMeshCount = meshIndirectData[i - 1].SubmeshCount;
				uint32_t currentMeshOffset = previousMeshOffset + previousMeshCount;

				uint32_t previousMaterialOffset = meshIndirectData[i - 1].MaterialOffset;
				uint32_t previousMaterialCount = meshIndirectData[i - 1].MaterialCount;
				uint32_t currentMaterialOffset = previousMaterialOffset + previousMaterialCount;

				meshIndirectData.emplace_back(IndirectMeshData(currentMeshOffset, submeshes.size(), i, mesh->GetMaterialCount(), currentMaterialOffset));
			}


		}

		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);


		
		Voxelization_Update(renderQueue);

		// Bind the pipeline and renderpass
		m_Data->VoxelizationRenderPass->Bind();
		m_Data->VoxelizationPipeline->Bind();

		// Set the viewport and scrissors
		VkViewport viewport{};
		viewport.width = (float)VoxelVolumeDimensions;
		viewport.height = (float)VoxelVolumeDimensions;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { (uint32_t)VoxelVolumeDimensions, (uint32_t)VoxelVolumeDimensions };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);


		// TODO: This is so bad, pls fix this
		//VkPipelineLayout pipelineLayout = m_Data->VoxelizationPipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();
		//
		//auto vulkanDescriptor = m_Data->VoxelizationDescriptor[currentFrameIndex].As<VulkanMaterial>();
		////vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		//Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		//
		//vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
		//m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);


		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->VoxelizationPipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->VoxelizationDescriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
		//m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);



		
#if 1


		// Sending the indirect draw commands to the command buffer
		for (uint32_t i = 0; i < meshIndirectData.size(); i++)
		{
			auto& meshData = meshIndirectData[i];
			uint32_t meshIndex = meshData.MeshIndex;

			// Get the mesh
			auto mesh = renderQueue.m_Data[meshIndex].Mesh;

			// Bind the index buffer
			mesh->GetIndexBuffer()->Bind();

			// Bind the vertex buffer (only the instanced one, since for the "per vertex" one, we are using BDAs
			auto vulkanVertexBufferInstanced = mesh->GetVertexBufferInstanced(currentFrameIndex).As<VulkanBufferDevice>();
			VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
			VkDeviceSize deviceSize[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);


			// Set the transform matrix and model matrix of the submesh into a constant buffer
			VulkanVoxelizationPass::PushConstant pushConstant;
			pushConstant.MaterialIndex = meshIndirectData[i].MaterialOffset;
			pushConstant.VertexBufferBDA = mesh->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			pushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

			uint32_t submeshCount = meshData.SubmeshCount;
			uint32_t offset = meshIndirectData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));

		}
#endif

		// End the renderpass
		m_Data->VoxelizationRenderPass->Unbind();
	}

	void VulkanVoxelizationPass::OnRenderDebug()
	{

	}

	void VulkanVoxelizationPass::OnResize(uint32_t width, uint32_t height)
	{

	}

	void VulkanVoxelizationPass::OnResizeLate(uint32_t width, uint32_t height)
	{

	}

	void VulkanVoxelizationPass::ShutDown()
	{
		delete m_Data;
	}

}