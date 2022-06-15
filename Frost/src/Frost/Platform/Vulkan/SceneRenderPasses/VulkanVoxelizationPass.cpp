#include "frostpch.h"
#include "VulkanVoxelizationPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"

#include <imgui.h>

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

		VoxelizationInit();
		ClearBufferInit();
	}

	void VulkanVoxelizationPass::VoxelizationInit()
	{
		// Getting all the needed information
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().GeometryPass_Mesh_Count;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		// RenderPass
		RenderPassSpecification renderPassSpecs =
		{
			(uint32_t)m_VoxelVolumeDimensions, (uint32_t)m_VoxelVolumeDimensions, 3,
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
		pipelineCreateInfo.ConservativeRasterization = true;
		pipelineCreateInfo.Cull = CullMode::None;
		pipelineCreateInfo.RenderPass = m_Data->VoxelizationRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data->VoxelizationPipeline = Pipeline::Create(pipelineCreateInfo);

		m_Data->VoxelizationTexture.resize(framesInFlight);
		m_Data->VoxelTexture_R32UI.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = m_VoxelVolumeDimensions;
			imageSpec.Height = m_VoxelVolumeDimensions;
			imageSpec.Depth = m_VoxelVolumeDimensions;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
			imageSpec.UseMipChain = false;
			imageSpec.MutableFormat = true;

			m_Data->VoxelizationTexture[i] = Texture3D::Create(imageSpec);


			Ref<VulkanTexture3D> vulkanVoxelTexture = m_Data->VoxelizationTexture[i].As<VulkanTexture3D>();

			VkImageUsageFlags usageFlags = Utils::GetImageUsageFlags(imageSpec.Usage);
			Utils::CreateImageView(m_Data->VoxelTexture_R32UI[i], vulkanVoxelTexture->GetVulkanImage(), usageFlags, VK_FORMAT_R32_UINT, 1, m_VoxelVolumeDimensions);
		}

		m_Data->VoxelizationDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->VoxelizationDescriptor[i] = Material::Create(m_Data->VoxelizationShader, "Voxelization_Material");

			auto& instanceSpec = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->MaterialSpecs[i];

			Ref<VulkanMaterial> descriptor = m_Data->VoxelizationDescriptor[i].As<VulkanMaterial>();
			VkDescriptorSet descriptorSet = descriptor->GetVulkanDescriptorSet(0);

			descriptor->Set("u_VoxelTexture_NonAtomic", m_Data->VoxelizationTexture[i]);
			descriptor->Set("u_MaterialUniform", instanceSpec.DeviceBuffer);
			descriptor->UpdateVulkanDescriptorIfNeeded();


			// Voxel texture with R32UI format
			Ref<VulkanTexture3D> vulkanVoxelTexture = m_Data->VoxelizationTexture[i].As<VulkanTexture3D>();
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = vulkanVoxelTexture->GetVulkanImageLayout();
			imageInfo.imageView = m_Data->VoxelTexture_R32UI[i];
			imageInfo.sampler = nullptr;

			VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			writeDescriptorSet.dstBinding = 2;
			writeDescriptorSet.dstArrayElement = 0;
			writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writeDescriptorSet.pImageInfo = &imageInfo;
			writeDescriptorSet.descriptorCount = 1;
			writeDescriptorSet.dstSet = descriptorSet;

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

		m_Data->IndirectVoxelCmdBuffer.resize(framesInFlight);
		for (auto& indirectCmdBuffer : m_Data->IndirectVoxelCmdBuffer)
		{
			// Allocating a heap block
			indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes, { BufferUsage::Storage, BufferUsage::Indirect });
			indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes);
		}
	}

	void VulkanVoxelizationPass::ClearBufferInit()
	{
		// Allocating a cpu buffer (same size as the 3d texture)
		uint32_t cpuBufferSize = std::pow(m_VoxelVolumeDimensions, 3) * 4;
		uint8_t* cpuBuffer = new uint8_t[cpuBufferSize];
		memset(cpuBuffer, 0, cpuBufferSize); // Setting everything to 0

		// Create a gpu buffer
		uint32_t clearBufferSize = std::pow(m_VoxelVolumeDimensions, 3) * sizeof(uint8_t) * 4;
		m_Data->ClearBuffer = BufferDevice::Create(clearBufferSize, { BufferUsage::TransferSrc });

		// Copy the data from the cpu buffer into the gpu
		m_Data->ClearBuffer->SetData(cpuBuffer);

		// Delete the cpu buffer
		delete cpuBuffer;
	}

	void VulkanVoxelizationPass::VoxelizationUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.layerCount = 1;
		region.imageSubresource.baseArrayLayer = 0;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			(uint32_t)m_VoxelVolumeDimensions,
			(uint32_t)m_VoxelVolumeDimensions,
			(uint32_t)m_VoxelVolumeDimensions
		};
		
		Ref<VulkanTexture3D> voxelTexture = m_Data->VoxelizationTexture[currentFrameIndex].As<VulkanTexture3D>();
		Ref<VulkanBufferDevice> clearBufferDevice = m_Data->ClearBuffer.As<VulkanBufferDevice>();
		
		voxelTexture->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		vkCmdCopyBufferToImage(cmdBuf, clearBufferDevice->GetVulkanBuffer(), voxelTexture->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		voxelTexture->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);




		float size = glm::ceil(m_Data->m_VoxelGrid * m_Data->m_VoxelSize);

		glm::vec3 camPos = renderQueue.CameraPosition;
		
		float camPosX = camPos.x;
		float camPosY = camPos.y;
		float camPosZ = camPos.z;


		// X
		glm::mat4 projectionMatrix_X = glm::ortho(
			(-(size) * 0.5f) - camPosZ, ((size) * 0.5f) - camPosZ,
			( -size * 0.5f)  - camPosY,  (size * 0.5f)  - camPosY,
			(size * 0.5f), (size * 1.5f)
		);
		projectionMatrix_X[1][1] *= -1.0f;

		// Y
		glm::mat4 projectionMatrix_Y = glm::ortho(
			(-(size) * 0.5f) + camPosX, ((size) * 0.5f) + camPosX,
			( -size * 0.5f)  + camPosZ,  (size * 0.5f)  + camPosZ,
			(size * 0.5f), (size * 1.5f)
		);
		projectionMatrix_Y[1][1] *= -1.0f;

		// Z
		glm::mat4 projectionMatrix_Z = glm::ortho(
			(-(size) * 0.5f) + camPosX, ((size) * 0.5f) + camPosX,
			( -size * 0.5f)  - camPosY,  (size * 0.5f)  - camPosY,
			(size * 0.5f), (size * 1.5f)
		);
		projectionMatrix_Z[1][1] *= -1.0f;

		glm::mat4 viewX = glm::lookAt(glm::vec3(size + camPosX, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
		glm::mat4 viewY = glm::lookAt(glm::vec3(0, size + camPosY, 0), glm::vec3(0, 0, 0), glm::vec3(0, 0, -1));
		glm::mat4 viewZ = glm::lookAt(glm::vec3(0, 0, size + camPosZ), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		VoxelProj.X = projectionMatrix_X * viewX;
		VoxelProj.Y = projectionMatrix_Y * viewY;
		VoxelProj.Z = projectionMatrix_Z * viewZ;

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


		
		VoxelizationUpdate(renderQueue);

		// Bind the pipeline and renderpass
		m_Data->VoxelizationRenderPass->Bind();
		m_Data->VoxelizationPipeline->Bind();

		// Set the viewport and scrissors
		VkViewport viewport{};
		viewport.width = (float)m_VoxelVolumeDimensions;
		viewport.height = (float)m_VoxelVolumeDimensions;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { (uint32_t)m_VoxelVolumeDimensions, (uint32_t)m_VoxelVolumeDimensions };
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
			pushConstant.AtomicOperation = m_AtomicOperation;
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

			uint32_t submeshCount = meshData.SubmeshCount;
			uint32_t offset = meshIndirectData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));

		}

		// End the renderpass
		m_Data->VoxelizationRenderPass->Unbind();
	}

	void VulkanVoxelizationPass::OnRenderDebug()
	{
		if (ImGui::CollapsingHeader("Voxelization Pass"))
		{
			ImGui::SliderInt("Atomic Operation (bool)", &m_AtomicOperation, 0, 1);
			ImGui::DragFloat("Voxel Size", &m_Data->m_VoxelSize, 0.01f, 0.5f, 2.0f);
		}
	}

	void VulkanVoxelizationPass::OnResize(uint32_t width, uint32_t height)
	{

	}

	void VulkanVoxelizationPass::OnResizeLate(uint32_t width, uint32_t height)
	{

	}

	void VulkanVoxelizationPass::ShutDown()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		for (auto voxelTextureImageView : m_Data->VoxelTexture_R32UI)
		{
			vkDestroyImageView(device, voxelTextureImageView, nullptr);
		}

		delete m_Data;
	}

}