#include "frostpch.h"
#include "VulkanVoxelizationPass.h"

#include "Frost/Asset/AssetManager.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanUniformBuffer.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanShadowPass.h"

#include "Frost/Platform/Vulkan/VulkanImage.h"

#include <GLFW/glfw3.h>

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
		m_Data->VoxelFilterShader = Renderer::GetShaderLibrary()->Get("VoxelFilter");
		m_Data->VoxelConeTracingShader = Renderer::GetShaderLibrary()->Get("VoxelConeTracing");

		VoxelizationInit();
		ClearBufferInit();

		VoxelFilterInit();
		VoxelConeTracingInit(1600, 900);
	}

	void VulkanVoxelizationPass::VoxelizationInit()
	{
		// Getting all the needed information
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t voxelVolumeDimensions = Renderer::GetRendererConfig().VoxelTextureResolution;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		m_VoxelizationPushConstant.VoxelDimensions = voxelVolumeDimensions;
		m_Data->m_VoxelGrid = voxelVolumeDimensions;

		// RenderPass
		RenderPassSpecification renderPassSpecs =
		{
			(uint32_t)voxelVolumeDimensions, (uint32_t)voxelVolumeDimensions, 3,
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
			{ "a_ModelSpaceMatrix",    ShaderDataType::Mat4 },
			{ "a_WorldSpaceMatrix",    ShaderDataType::Mat4 },
			{ "a_MaterialIndexOffset", ShaderDataType::UInt },
		};
		bufferLayout.m_InputType = InputType::Instanced;

		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->VoxelizationShader;
		pipelineCreateInfo.UseDepthTest = false;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.UseStencil = false;
		pipelineCreateInfo.ConservativeRasterization = false;
		pipelineCreateInfo.Cull = CullMode::None;
		pipelineCreateInfo.RenderPass = m_Data->VoxelizationRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data->VoxelizationPipeline = Pipeline::Create(pipelineCreateInfo);

		m_Data->VoxelizationTexture.resize(framesInFlight);
		m_Data->VoxelTexture_R32UI.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = voxelVolumeDimensions;
			imageSpec.Height = voxelVolumeDimensions;
			imageSpec.Depth = voxelVolumeDimensions;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.UseMipChain = true;
			imageSpec.MutableFormat = true;

			m_Data->VoxelizationTexture[i] = Texture3D::Create(imageSpec);


			Ref<VulkanTexture3D> vulkanVoxelTexture = m_Data->VoxelizationTexture[i].As<VulkanTexture3D>();

			VkImageUsageFlags usageFlags = Utils::GetImageUsageFlags(imageSpec.Usage);
			Utils::CreateImageView(m_Data->VoxelTexture_R32UI[i], vulkanVoxelTexture->GetVulkanImage(), usageFlags, VK_FORMAT_R32_UINT, 1, voxelVolumeDimensions);
		}

		m_Data->VoxelizationDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->VoxelizationDescriptor[i] = Material::Create(m_Data->VoxelizationShader, "Voxelization_Material");

			auto instanceSpec = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->MaterialSpecs[i];

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
		m_Data->GlobalInstancedVertexBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			auto& indirectCmdBuffer = m_Data->IndirectVoxelCmdBuffer[i];

			// Allocating a heap block
			indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes, { BufferUsage::Storage, BufferUsage::Indirect });
			indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes);


			auto& instancdVertexBuffer = m_Data->GlobalInstancedVertexBuffer[i];

			// Allocating a heap block
			instancdVertexBuffer.DeviceBuffer = BufferDevice::Create(sizeof(MeshInstancedVertexBuffer) * maxCountMeshes, { BufferUsage::Vertex });
			instancdVertexBuffer.HostBuffer.Allocate(sizeof(MeshInstancedVertexBuffer) * maxCountMeshes);
		}
	}

	void VulkanVoxelizationPass::ClearBufferInit()
	{
		uint32_t voxelVolumeDimensions = Renderer::GetRendererConfig().VoxelTextureResolution;

		// Allocating a cpu buffer (same size as the 3d texture)
		uint32_t cpuBufferSize = std::pow(voxelVolumeDimensions, 3) * 4;
		uint8_t* cpuBuffer = new uint8_t[cpuBufferSize];
		memset(cpuBuffer, 0, cpuBufferSize); // Setting everything to 0

		// Create a gpu buffer
		uint32_t clearBufferSize = std::pow(voxelVolumeDimensions, 3) * sizeof(uint8_t) * 4;
		m_Data->ClearBuffer = BufferDevice::Create(clearBufferSize, { BufferUsage::TransferSrc });

		// Copy the data from the cpu buffer into the gpu
		m_Data->ClearBuffer->SetData(cpuBuffer);

		// Delete the cpu buffer
		delete cpuBuffer;
	}

	void VulkanVoxelizationPass::UpdateRenderingSettings()
	{
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		m_VCTPushConstant.UseIndirectDiffuse = rendererSettings.VoxelGI.UseIndirectDiffuse;
		m_VCTPushConstant.UseIndirectSpecular = rendererSettings.VoxelGI.UseIndirectSpecular;
		m_VCTPushConstant.ConeTraceMaxSteps = rendererSettings.VoxelGI.ConeTraceMaxSteps;
		m_VCTPushConstant.ConeTraceMaxDistance = rendererSettings.VoxelGI.ConeTraceMaxDistance;
	}

	void VulkanVoxelizationPass::VoxelFilterInit()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		
		// Voxel Filter Compute pipeline
		ComputePipeline::CreateInfo createInfo{};
		createInfo.Shader = m_Data->VoxelFilterShader;
		m_Data->VoxelFilterPipeline = ComputePipeline::Create(createInfo);

		m_Data->VoxelFilterDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			m_Data->VoxelFilterDescriptor[i] = Material::Create(m_Data->VoxelFilterShader, "VoxelFilter");
		}
	}

	void VulkanVoxelizationPass::InitLate()
	{
	}

	void VulkanVoxelizationPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;

		RendererSettings& rendererSettings = Renderer::GetRendererSettings();
		UpdateRenderingSettings();

		if (rendererSettings.VoxelGI.EnableVoxelization)
		{
			VulkanRenderer::BeginTimeStampPass("Voxelization Pass");
			//VoxelizationUpdateRendering(renderQueue);
			VoxelizationUpdateRenderingWithInstancing(renderQueue);
			VulkanRenderer::EndTimeStampPass("Voxelization Pass");

			VulkanRenderer::BeginTimeStampPass("Voxel Filter Pass");
			VoxelFilterUpdate(renderQueue);
			VulkanRenderer::EndTimeStampPass("Voxel Filter Pass");
		}

		VulkanRenderer::BeginTimeStampPass("Voxel Cone Tracing Pass");
		VoxelConeTracingUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("Voxel Cone Tracing Pass");
	}

	void VulkanVoxelizationPass::VoxelConeTracingInit(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		// Setting up the voxel volume texture at the start (since this wont get changed during run time)
		float voxelTextureSize = Renderer::GetRendererConfig().VoxelTextureResolution;
		m_VCTPushConstant.VoxelTextureSize = voxelTextureSize;

		// Voxel Filter Compute pipeline
		ComputePipeline::CreateInfo createInfo{};
		createInfo.Shader = m_Data->VoxelConeTracingShader;
		if (!m_Data->VoxelConeTracingPipeline)
			m_Data->VoxelConeTracingPipeline = ComputePipeline::Create(createInfo);

		m_Data->VCT_IndirectDiffuseTexture.resize(framesInFlight);
		m_Data->VCT_IndirectSpecularTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width;
			imageSpec.Height = height;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

			m_Data->VCT_IndirectDiffuseTexture[i] = Image2D::Create(imageSpec);
			m_Data->VCT_IndirectSpecularTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->VoxelConeTracingDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->VoxelConeTracingDescriptor[i])
				m_Data->VoxelConeTracingDescriptor[i] = Material::Create(m_Data->VoxelConeTracingShader, "VoxelConeTracing");

			auto descriptor = m_Data->VoxelConeTracingDescriptor[i].As<VulkanMaterial>();

			auto positionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);
			auto normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(1, i);

			descriptor->Set("u_VoxelTexture", m_Data->VoxelizationTexture[i]);
			descriptor->Set("u_PositionTexture", positionTexture);
			descriptor->Set("u_NormalTexture", normalTexture);
			descriptor->Set("u_IndirectDiffuseTexture", m_Data->VCT_IndirectDiffuseTexture[i]);
			descriptor->Set("u_IndirectSpecularTexture", m_Data->VCT_IndirectSpecularTexture[i]);

			descriptor->UpdateVulkanDescriptorIfNeeded();

		}
	}

	void VulkanVoxelizationPass::VoxelizationUpdateData(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		uint32_t voxelVolumeDimensions = Renderer::GetRendererConfig().VoxelTextureResolution;
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

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
			voxelVolumeDimensions,
			voxelVolumeDimensions,
			voxelVolumeDimensions
		};

		Ref<VulkanTexture3D> voxelTexture = m_Data->VoxelizationTexture[currentFrameIndex].As<VulkanTexture3D>();
		Ref<VulkanBufferDevice> clearBufferDevice = m_Data->ClearBuffer.As<VulkanBufferDevice>();

		voxelTexture->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vkCmdCopyBufferToImage(cmdBuf, clearBufferDevice->GetVulkanBuffer(), voxelTexture->GetVulkanImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		voxelTexture->TransitionLayout(cmdBuf, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);




		float size = glm::round(m_Data->m_VoxelGrid * rendererSettings.VoxelGI.VoxelSize);
		size = size + float(int32_t(size) % 2);
		m_Data->m_VoxelAABB = size;

		glm::vec3 camPos = renderQueue.CameraPosition;

		float camPosX = glm::round(camPos.x);
		float camPosY = glm::round(camPos.y);
		float camPosZ = glm::round(camPos.z);


		int32_t offsetFactor = int32_t(8.0f * rendererSettings.VoxelGI.VoxelSize);

		camPosX = camPosX + abs(int32_t(camPosX) % offsetFactor - offsetFactor);
		camPosY = camPosY + abs(int32_t(camPosY) % offsetFactor - offsetFactor);
		camPosZ = camPosZ + abs(int32_t(camPosZ) % offsetFactor - offsetFactor);

		//FROST_CORE_INFO("X: {0}  Y: {1}   Z: {2}", camPosX, camPosY, camPosZ);

		m_Data->VoxelCameraPosition = { camPosX, camPosY, camPosZ };


		// X
		glm::mat4 projectionMatrix_X = glm::ortho(
			(-(size) * 0.5f) - camPosZ, ((size) * 0.5f) - camPosZ,
			(-size * 0.5f) - camPosY, (size * 0.5f) - camPosY,
			(size * 0.5f), (size * 1.5f)
		);
		projectionMatrix_X[1][1] *= -1.0f;

		// Y
		glm::mat4 projectionMatrix_Y = glm::ortho(
			(-(size) * 0.5f) + camPosX, ((size) * 0.5f) + camPosX,
			(-size * 0.5f) + camPosZ, (size * 0.5f) + camPosZ,
			(size * 0.5f), (size * 1.5f)
		);
		projectionMatrix_Y[1][1] *= -1.0f;

		// Z
		glm::mat4 projectionMatrix_Z = glm::ortho(
			(-(size) * 0.5f) + camPosX, ((size) * 0.5f) + camPosX,
			(-size * 0.5f) - camPosY, (size * 0.5f) - camPosY,
			(size * 0.5f), (size * 1.5f)
		);
		projectionMatrix_Z[1][1] *= -1.0f;

		glm::mat4 viewX = glm::lookAt(glm::vec3(size + camPosX, 0, 0), glm::vec3(camPosX, 0, 0), glm::vec3(0, 1, 0));
		glm::mat4 viewY = glm::lookAt(glm::vec3(0, size + camPosY, 0), glm::vec3(0, camPosY, 0), glm::vec3(0, 0, -1));
		glm::mat4 viewZ = glm::lookAt(glm::vec3(0, 0, size + camPosZ), glm::vec3(0, 0, camPosZ), glm::vec3(0, 1, 0));

		m_VoxelAABBProjection.X = projectionMatrix_X * viewX;
		m_VoxelAABBProjection.Y = projectionMatrix_Y * viewY;
		m_VoxelAABBProjection.Z = projectionMatrix_Z * viewZ;

		m_Data->VoxelizationDescriptor[currentFrameIndex]->Set("VoxelProjections.AxisX", m_VoxelAABBProjection.X);
		m_Data->VoxelizationDescriptor[currentFrameIndex]->Set("VoxelProjections.AxisY", m_VoxelAABBProjection.Y);
		m_Data->VoxelizationDescriptor[currentFrameIndex]->Set("VoxelProjections.AxisZ", m_VoxelAABBProjection.Z);

#if 0
		// https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiRenderer.cpp#L3135



		// Update Voxelization parameters :
	https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiRenderer.cpp#L2953
		if (scene.objects.GetCount() > 0)
		{
			// We don't update it if the scene is empty, this even makes it easier to debug
			const float f = 0.05f / voxelSceneData.voxelsize;
			XMFLOAT3 center = XMFLOAT3(std::floor(vis.camera->Eye.x * f) / f, std::floor(vis.camera->Eye.y * f) / f, std::floor(vis.camera->Eye.z * f) / f);
			if (wi::math::DistanceSquared(center, voxelSceneData.center) > 0)
			{
				voxelSceneData.centerChangedThisFrame = true;
			}
			else
			{
				voxelSceneData.centerChangedThisFrame = false;
			}
			voxelSceneData.center = center;
			voxelSceneData.extents = XMFLOAT3(voxelSceneData.res * voxelSceneData.voxelsize, voxelSceneData.res * voxelSceneData.voxelsize, voxelSceneData.res * voxelSceneData.voxelsize);
		}
#endif
	}


	struct MeshInstanceListVoxelizationPass // This is reponsible for grouping all the mesh instances into one array
	{
		Mesh* Mesh; // Getting it as a raw pointer to intefere with the reference count
		glm::mat4 Transform;
		uint32_t MeshIndex;
	};

	static Vector<NewIndirectMeshData> s_VoxelizationMeshIndirectData;
	static HashMap<AssetHandle, Vector<MeshInstanceListVoxelizationPass>> s_GroupedMeshesCached;

#if 0
	void VulkanVoxelizationPass::VoxelizationUpdateRendering(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->VoxelizationRenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->VoxelizationPipeline.As<VulkanPipeline>();

		// Get the needed matricies from the camera
		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectionMatrix * renderQueue.CameraViewMatrix;

		VoxelizationUpdateData(renderQueue);

		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
		*/
		s_VoxelizationMeshIndirectData.clear();

		// `Indirect draw commands` offsets
		uint64_t indirectCmdsOffset = 0;

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetMeshAsset()->GetSubMeshes();

			//if (mesh->IsAnimated()) continue;

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
			if (s_VoxelizationMeshIndirectData.size() == 0)
			{
				s_VoxelizationMeshIndirectData.emplace_back(IndirectMeshData(0, submeshes.size(), i, mesh->GetMaterialCount(), 0));
			}
			else
			{
				uint32_t previousMeshOffset = s_VoxelizationMeshIndirectData[i - 1].SubmeshOffset;
				uint32_t previousMeshCount = s_VoxelizationMeshIndirectData[i - 1].SubmeshCount;
				uint32_t currentMeshOffset = previousMeshOffset + previousMeshCount;

				uint32_t previousMaterialOffset = s_VoxelizationMeshIndirectData[i - 1].MaterialOffset;
				uint32_t previousMaterialCount = s_VoxelizationMeshIndirectData[i - 1].MaterialCount;
				uint32_t currentMaterialOffset = previousMaterialOffset + previousMaterialCount;

				s_VoxelizationMeshIndirectData.emplace_back(IndirectMeshData(currentMeshOffset, submeshes.size(), i, mesh->GetMaterialCount(), currentMaterialOffset));
			}


		}

		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);


		Ref<VulkanImage2D> shadowDepthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowDepthRenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
		shadowDepthTexture->TransitionLayout(cmdBuf, shadowDepthTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);



		// Bind the pipeline and renderpass
		m_Data->VoxelizationRenderPass->Bind();
		m_Data->VoxelizationPipeline->Bind();

		// Set the viewport and scrissors
		VkViewport viewport{};
		viewport.width = (float)m_Data->VoxelizationRenderPass->GetColorAttachment(0, currentFrameIndex)->GetWidth();
		viewport.height = (float)m_Data->VoxelizationRenderPass->GetColorAttachment(0, currentFrameIndex)->GetHeight();
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);



		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->VoxelizationPipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->VoxelizationDescriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
		//m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);




		// Sending the indirect draw commands to the command buffer
		for (uint32_t i = 0; i < s_VoxelizationMeshIndirectData.size(); i++)
		{
			auto& meshData = s_VoxelizationMeshIndirectData[i];
			uint32_t meshIndex = meshData.MeshIndex;

			// Get the mesh
			auto mesh = renderQueue.m_Data[meshIndex].Mesh;

			// Bind the index buffer
			mesh->GetMeshAsset()->GetIndexBuffer()->Bind();

			// Bind the vertex buffer (only the instanced one, since for the "per vertex" one, we are using BDAs
			auto vulkanVertexBufferInstanced = mesh->GetVertexBufferInstanced(currentFrameIndex).As<VulkanBufferDevice>();
			VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
			VkDeviceSize deviceSize[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);


			// Set the transform matrix and model matrix of the submesh into a constant buffer
			m_VoxelizationPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
			m_VoxelizationPushConstant.MaterialIndex = s_VoxelizationMeshIndirectData[i].MaterialOffset;
			m_VoxelizationPushConstant.VertexBufferBDA = mesh->GetMeshAsset()->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_VoxelizationPushConstant);

			uint32_t submeshCount = meshData.SubmeshCount;
			uint32_t offset = s_VoxelizationMeshIndirectData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));

		}

		// End the renderpass
		m_Data->VoxelizationRenderPass->Unbind();
	}
#endif

	void VulkanVoxelizationPass::VoxelizationUpdateRenderingWithInstancing(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->VoxelizationRenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->VoxelizationPipeline.As<VulkanPipeline>();

		// Get the needed matricies from the camera
		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectionMatrix * renderQueue.CameraViewMatrix;

		VoxelizationUpdateData(renderQueue);

		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
			(TODO: This explanation is outdated, now we are using `instanced indirect multidraw rendering` :D)
		*/
		s_VoxelizationMeshIndirectData.clear();
		s_GroupedMeshesCached.clear();

		// Allocate all the neccesary array buffers before, so we won't waste cpus cycles on reallocating memory
		for (auto& [meshAssetUUID, instanceCount] : renderQueue.m_MeshInstanceCount)
		{
			s_GroupedMeshesCached[meshAssetUUID].reserve(instanceCount);
		}
		s_VoxelizationMeshIndirectData.reserve(s_GroupedMeshesCached.size());


		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;

			s_GroupedMeshesCached[mesh->GetMeshAsset()->Handle].push_back({ mesh.Raw(), renderQueue.m_Data[i].Transform, i});
		}

		// `Indirect draw commands` offset
		uint64_t indirectCmdsOffset = 0;

		// `Instance data` offset.
		//uint64_t materialDataOffset = 0;

		// `Instance data` offset.
		uint64_t instanceVertexOffset = 0;

		for (auto& [handle, groupedMeshes] : s_GroupedMeshesCached)
		{
			NewIndirectMeshData* currentIndirectMeshData;
			NewIndirectMeshData* lastIndirectMeshData = nullptr;
			
			// If we are submitting the first mesh, we don't need any offset
			currentIndirectMeshData = &s_VoxelizationMeshIndirectData.emplace_back();

			// We are checking `.size() > 1` and also `.size() - 2` because we've already pushed_back and element before, so we should go 2 elements behind instead of one
			if (s_VoxelizationMeshIndirectData.size() > 1)
				lastIndirectMeshData = &s_VoxelizationMeshIndirectData[s_VoxelizationMeshIndirectData.size() - 2];


			Ref<MeshAsset> meshAsset = groupedMeshes[0].Mesh->GetMeshAsset();
			const Vector<Submesh>& submeshes = meshAsset->GetSubMeshes();

			currentIndirectMeshData->MeshAssetHandle = meshAsset->Handle;
			currentIndirectMeshData->InstanceCount = groupedMeshes.size();
			currentIndirectMeshData->SubmeshCount = submeshes.size();
			currentIndirectMeshData->TotalSubmeshCount = currentIndirectMeshData->SubmeshCount * currentIndirectMeshData->InstanceCount;
			currentIndirectMeshData->MaterialCount = groupedMeshes[0].Mesh->GetMaterialCount();

			currentIndirectMeshData->CmdOffset = indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand);

			currentIndirectMeshData->MaterialOffset = 0;
			currentIndirectMeshData->TotalMeshOffset = 0;
			if (s_VoxelizationMeshIndirectData.size() > 1)
			{
				currentIndirectMeshData->MaterialOffset = lastIndirectMeshData->MaterialOffset + (lastIndirectMeshData->MaterialCount * lastIndirectMeshData->InstanceCount);
				currentIndirectMeshData->TotalMeshOffset = lastIndirectMeshData->TotalMeshOffset + (lastIndirectMeshData->SubmeshCount * lastIndirectMeshData->InstanceCount);
			}

			// Set up the instanced vertex buffer (per submesh, per instance)
			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); submeshIndex++)
			{
				MeshInstancedVertexBuffer meshInstancedVertexBuffer{};
				uint32_t meshInstanceNr = 0;
				for (auto& meshInstance : groupedMeshes)
				{
					glm::mat4 modelMatrix = meshInstance.Transform * submeshes[submeshIndex].Transform;

					 // Using skeletal (dynamic) submesh transforms, instead of the static ones which are found in the mesh asset
					//glm::mat4 modelMatrix = meshInstance.Transform * meshInstance.Mesh->GetSkeletalSubmeshes()[submeshIndex].Transform;


					// Adding the neccesary Matricies for the shader
					meshInstancedVertexBuffer.ModelSpaceMatrix = modelMatrix;
					meshInstancedVertexBuffer.WorldSpaceMatrix = viewProjectionMatrix * modelMatrix;
					/////////////////////////////////////////////////////

					// Doing `MaterialOffset` because we are indicating it to the whole material buffer (so the index should be global)
					meshInstancedVertexBuffer.MaterialIndexOffset = currentIndirectMeshData->MaterialOffset + (meshInstanceNr * currentIndirectMeshData->MaterialCount);
					/////////////////////////////////////////////////////

					m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].HostBuffer.Write((void*)&meshInstancedVertexBuffer, sizeof(MeshInstancedVertexBuffer), instanceVertexOffset);
					instanceVertexOffset += sizeof(MeshInstancedVertexBuffer);

					meshInstanceNr++;
				}
			}

			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); submeshIndex++)
			{
				const Submesh& submesh = submeshes[submeshIndex];

				// Submit the submesh into the cpu buffer
				VkDrawIndexedIndirectCommand indirectCmdBuf{};
				indirectCmdBuf.firstIndex = submesh.BaseIndex;
				indirectCmdBuf.indexCount = submesh.IndexCount;
				indirectCmdBuf.vertexOffset = submesh.BaseVertex;

				indirectCmdBuf.instanceCount = groupedMeshes.size();

				if (lastIndirectMeshData)
				{
					indirectCmdBuf.firstInstance = currentIndirectMeshData->TotalMeshOffset;
				}
				else
				{
					indirectCmdBuf.firstInstance = 0;
				}
				indirectCmdBuf.firstInstance += submeshIndex;

				m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
			}
		}

		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);

		// Global Instanced Vertex Buffer data
		auto vulkanInstancedVertexBuffer = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* instancedVertexBufferPointer = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanInstancedVertexBuffer->SetData(instanceVertexOffset, instancedVertexBufferPointer);


		Ref<VulkanImage2D> shadowDepthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowDepthRenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
		shadowDepthTexture->TransitionLayout(cmdBuf, shadowDepthTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);



		// Bind the pipeline and renderpass
		m_Data->VoxelizationRenderPass->Bind();
		m_Data->VoxelizationPipeline->Bind();

		// Set the viewport and scrissors
		VkViewport viewport{};
		viewport.width = (float)m_Data->VoxelizationRenderPass->GetColorAttachment(0, currentFrameIndex)->GetWidth();
		viewport.height = (float)m_Data->VoxelizationRenderPass->GetColorAttachment(0, currentFrameIndex)->GetHeight();
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { (uint32_t)viewport.width, (uint32_t)viewport.height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);



		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->VoxelizationPipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->VoxelizationDescriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
		//m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);


		// Binding the global instanced vertex buffer only once
		auto vulkanVertexBufferInstanced = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
		VkDeviceSize deviceSize[1] = { 0 };
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);


		// Sending the indirect draw commands to the command buffer
		for (uint32_t i = 0; i < s_VoxelizationMeshIndirectData.size(); i++)
		{
			auto& indirectPerMeshData = s_VoxelizationMeshIndirectData[i];

			// Get the mesh
			const AssetMetadata& assetMetadata = AssetManager::GetMetadata(indirectPerMeshData.MeshAssetHandle);
			Ref<MeshAsset> meshAsset = AssetManager::GetAsset<MeshAsset>(assetMetadata.FilePath.string());

			// Bind the index buffer
			meshAsset->GetIndexBuffer()->Bind();


			// Set the transform matrix and model matrix of the submesh into a constant buffer
			m_VoxelizationPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
			//m_VoxelizationPushConstant.MaterialIndex = s_VoxelizationMeshIndirectData[i].MaterialOffset;
			m_VoxelizationPushConstant.VertexBufferBDA = meshAsset->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_VoxelizationPushConstant);

			uint32_t submeshCount = indirectPerMeshData.SubmeshCount;
			uint32_t offset = indirectPerMeshData.CmdOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));

		}

		// End the renderpass
		m_Data->VoxelizationRenderPass->Unbind();
	}

	void VulkanVoxelizationPass::VoxelFilterUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		Ref<VulkanMaterial> vulkanDescriptor = m_Data->VoxelFilterDescriptor[currentFrameIndex].As<VulkanMaterial>();
		Ref<VulkanComputePipeline> vulkanPipeline = m_Data->VoxelFilterPipeline.As<VulkanComputePipeline>();


		m_VoxelFilterPushConstant.ProjectionExtents = m_Data->m_VoxelAABB / 2.0f;
		m_VoxelFilterPushConstant.VoxelScale = rendererSettings.VoxelGI.VoxelSize;

		m_VoxelFilterPushConstant.CameraPosition_SampleMipLevel = glm::vec4(m_Data->VoxelCameraPosition, 1.0f);
		m_VoxelFilterPushConstant.CameraViewMatrix = renderQueue.CameraViewMatrix;


		// Making a barrier to be sure that the voxelization pass has been finished
		Ref<VulkanTexture3D> vulkanVoxelTexture = m_Data->VoxelizationTexture[currentFrameIndex].As<VulkanTexture3D>();
		vulkanVoxelTexture->TransitionLayout(cmdBuf, vulkanVoxelTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		// Making a barrier to be sure that the shadow pass has been finished
		Ref<VulkanImage2D> shadowDepthTexture = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>()->ShadowDepthRenderPass->GetDepthAttachment(currentFrameIndex).As<VulkanImage2D>();
		shadowDepthTexture->TransitionLayout(cmdBuf, shadowDepthTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		// Shadow Cascades data neede for the shader
		auto shadowPassInternalData = m_RenderPassPipeline->GetRenderPassData<VulkanShadowPass>();

		glm::vec4 cascadeDepthSplit = {
			shadowPassInternalData->CascadeDepthSplit[0],
			shadowPassInternalData->CascadeDepthSplit[1],
			shadowPassInternalData->CascadeDepthSplit[2],
			shadowPassInternalData->CascadeDepthSplit[3]
		};

		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix0", shadowPassInternalData->CascadeViewProjMatrix[0]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix1", shadowPassInternalData->CascadeViewProjMatrix[1]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix2", shadowPassInternalData->CascadeViewProjMatrix[2]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix3", shadowPassInternalData->CascadeViewProjMatrix[3]);
		vulkanDescriptor->Set("DirectionaLightData.CascadeDepthSplit", cascadeDepthSplit);
		vulkanDescriptor->Set("DirectionaLightData.DirectionalLightIntensity", renderQueue.m_LightData.DirLight.Specification.Intensity);
		
		Ref<VulkanUniformBuffer> vulkanUniformBuffer = vulkanDescriptor->GetUniformBuffer("DirectionaLightData").As<VulkanUniformBuffer>();

		uint32_t mipWidth = vulkanVoxelTexture->GetWidth();
		uint32_t mipHeight = vulkanVoxelTexture->GetHeight();
		uint32_t mipDepth = vulkanVoxelTexture->GetDepth();

		for (uint32_t mip = 0; mip < vulkanVoxelTexture->GetMipChainLevels() - 2; mip++)
		{

			VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &vulkanDescriptor->GetVulkanDescriptorLayout()[0];
			VkDescriptorSet descriptorSet = VulkanRenderer::AllocateDescriptorSet(allocInfo);


			m_VoxelFilterPushConstant.CameraPosition_SampleMipLevel.w = mip;

			// Voxel Sampler
			{
				uint32_t sampleMip = (int32_t(mip) - 1) < 0 ? 0 : (mip - 1);

				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = vulkanVoxelTexture->GetVulkanImageLayout();
				imageInfo.imageView = vulkanVoxelTexture->GetVulkanImageViewMip(sampleMip);
				imageInfo.sampler = vulkanVoxelTexture->GetVulkanSampler();

				VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptorSet.dstBinding = 1;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSet.pImageInfo = &imageInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}

			// Voxel Image
			{
				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = vulkanVoxelTexture->GetVulkanImageLayout();
				imageInfo.imageView = vulkanVoxelTexture->GetVulkanImageViewMip(mip);
				imageInfo.sampler = nullptr;

				VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptorSet.dstBinding = 0;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writeDescriptorSet.pImageInfo = &imageInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}

			// Shadow Map
			{
				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = shadowDepthTexture->GetVulkanImageLayout();
				imageInfo.imageView = shadowDepthTexture->GetVulkanImageView();
				imageInfo.sampler = shadowDepthTexture->GetVulkanSampler();

				VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptorSet.dstBinding = 2;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSet.pImageInfo = &imageInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}

			// Shadow Cascades Uniform Buffer
			{
				VkDescriptorBufferInfo* bufferInfo = &vulkanUniformBuffer->GetVulkanDescriptorInfo();

				VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptorSet.dstBinding = 3;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSet.pBufferInfo = bufferInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
			}



			//vulkanDescriptor->Bind(cmdBuf, m_Data->VoxelFilterPipeline);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanPipeline->GetVulkanPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

			vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_VoxelFilterPushConstant);

			uint32_t groupX = glm::ceil(mipWidth / 8.0f);
			uint32_t groupY = glm::ceil(mipHeight / 8.0f);
			uint32_t groupZ = glm::ceil(mipDepth / 8.0f);
			vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, groupZ);


			// Set a barrier
			VkImageSubresourceRange imageSubrange{};
			imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageSubrange.baseArrayLayer = 0;
			imageSubrange.baseMipLevel = mip;
			imageSubrange.layerCount = 1;
			imageSubrange.levelCount = 1;

			Utils::InsertImageMemoryBarrier(cmdBuf, vulkanVoxelTexture->GetVulkanImage(),
				VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
				vulkanVoxelTexture->GetVulkanImageLayout(), vulkanVoxelTexture->GetVulkanImageLayout(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				imageSubrange
			);


			mipWidth /= 2;
			mipHeight /= 2;
			mipDepth /= 2;
		}

	}

	void VulkanVoxelizationPass::VoxelConeTracingUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		Ref<VulkanMaterial> vulkanDescriptor = m_Data->VoxelConeTracingDescriptor[currentFrameIndex].As<VulkanMaterial>();
		Ref<VulkanComputePipeline> vulkanPipeline = m_Data->VoxelConeTracingPipeline.As<VulkanComputePipeline>();

		m_VCTPushConstant.CameraPosition = renderQueue.CameraPosition;
		m_VCTPushConstant.VoxelSampleOffset = m_Data->VoxelCameraPosition;
		m_VCTPushConstant.VoxelGrid = glm::round(m_Data->m_VoxelGrid * rendererSettings.VoxelGI.VoxelSize);

		vulkanDescriptor->Bind(cmdBuf, m_Data->VoxelConeTracingPipeline);
		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_VCTPushConstant);

		uint32_t groupX = std::ceil((renderQueue.ViewPortWidth ) / 32.0f);
		uint32_t groupY = std::ceil((renderQueue.ViewPortHeight) / 32.0f);
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		Ref<VulkanImage2D> vulkanIndirectSpecularTexture = m_Data->VCT_IndirectSpecularTexture[currentFrameIndex].As<VulkanImage2D>();

		vulkanIndirectSpecularTexture->TransitionLayout(cmdBuf,
			vulkanIndirectSpecularTexture->GetVulkanImageLayout(),
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}


	void VulkanVoxelizationPass::OnRenderDebug()
	{
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();
		if (ImGui::CollapsingHeader("Voxelization Pass (Experimental)"))
		{
			ImGui::SliderInt("Enable Voxelization", &rendererSettings.VoxelGI.EnableVoxelization, 0, 1);
			ImGui::Separator();
			ImGui::SliderInt("Atomic Operation (bool)", &m_VoxelizationPushConstant.AtomicOperation, 0, 1);
			ImGui::DragFloat("Voxel Size", &rendererSettings.VoxelGI.VoxelSize, 0.25f, 0.5f, 2.0f);
			//ImGui::SliderInt("Cone Tracing Max Steps", &m_VCTPushConstant.ConeTraceMaxSteps, 0, 200);
			//ImGui::DragFloat("Cone Tracing Max Distance", &m_VCTPushConstant.ConeTraceMaxDistance, 0.01f, 0, 1000);
			ImGui::Separator();
			ImGui::SliderInt("Indirect Diffuse", &rendererSettings.VoxelGI.UseIndirectDiffuse, 0, 1);
			ImGui::SliderInt("Indirect Specular", &rendererSettings.VoxelGI.UseIndirectSpecular, 0, 1);
		}
	}

	void VulkanVoxelizationPass::OnResize(uint32_t width, uint32_t height)
	{
		VoxelConeTracingInit(width, height);

#if 0
		Renderer::SubmitImageToOutputMap("VXGI Diffuse", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->VCT_IndirectDiffuseTexture[currentFrameIndex];
		});

		Renderer::SubmitImageToOutputMap("VXGI Specular", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->VCT_IndirectSpecularTexture[currentFrameIndex];
		});
#endif
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