#include "frostpch.h"
#include "VulkanShadowPass.h"

#include "Frost/Core/Application.h"
#include "Frost/Asset/AssetManager.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"
#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanUniformBuffer.h"
#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanGeometryPass.h"

#include <imgui.h>

namespace Frost
{
	VulkanShadowPass::VulkanShadowPass()
		: m_Name("VulkanShadowPass")
	{

	}

	VulkanShadowPass::~VulkanShadowPass()
	{

	}

	void VulkanShadowPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->ShadowDepthShader = Renderer::GetShaderLibrary()->Get("ShadowDepthPass");
		m_Data->ShadowComputeShader = Renderer::GetShaderLibrary()->Get("ShadowCompute");
		m_Data->ShadowComputeDenoiseShader = Renderer::GetShaderLibrary()->Get("SpatialDenoiser");

		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;

		m_Data->IndirectShadowCmdBuffer.resize(framesInFlight);
		for (auto& indirectCmdBuffer : m_Data->IndirectShadowCmdBuffer)
		{
			// Allocating a heap block
			indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes, { BufferUsage::Storage, BufferUsage::Indirect });
			indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes);
		}

		/// Global Instaced Vertex Buffer
		m_Data->GlobalInstancedVertexBuffer.resize(framesInFlight);
		for (uint32_t i = 0; i < m_Data->GlobalInstancedVertexBuffer.size(); i++)
		{
			auto& instancdVertexBuffer = m_Data->GlobalInstancedVertexBuffer[i];

			// Allocating a heap block
			instancdVertexBuffer.DeviceBuffer = BufferDevice::Create(sizeof(MeshInstancedVertexBuffer) * maxCountMeshes, { BufferUsage::Vertex });
			instancdVertexBuffer.HostBuffer.Allocate(sizeof(MeshInstancedVertexBuffer) * maxCountMeshes);
		}

		ShadowDepthInitData();
		ShadowComputeInitData(1600, 900);
		ShadowComputeDenoiseInitData(1600, 900);
		CalculateCascadeOffsets();

		Renderer::SubmitImageToOutputImageMap("Shadows", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->ShadowComputeDenoiseTexture[currentFrameIndex];
		});
	}

	void VulkanShadowPass::ShadowDepthInitData()
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		uint32_t shadowTextureRes = Renderer::GetRendererConfig().ShadowTextureResolution;

		RenderPassSpecification renderPassSpecs =
		{
			shadowTextureRes * 2, shadowTextureRes * 2, framesInFlight,
			{
				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				}
			}
		};
		m_Data->ShadowDepthRenderPass = RenderPass::Create(renderPassSpecs);


		BufferLayout bufferLayout = {
			{ "a_ModelSpaceMatrix",           ShaderDataType::Mat4   },
			{ "a_WorldSpaceMatrix",           ShaderDataType::Mat4   },
			{ "a_BoneInformationBDA",         ShaderDataType::UInt64 }
		};
		bufferLayout.m_InputType = InputType::Instanced;

		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->ShadowDepthShader;
		pipelineCreateInfo.RenderPass = m_Data->ShadowDepthRenderPass;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.DepthCompareOperation = DepthCompare::LessOrEqual;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data->ShadowDepthPipeline = Pipeline::Create(pipelineCreateInfo);
	}

	void VulkanShadowPass::ShadowComputeInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		ComputePipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->ShadowComputeShader;
		if (!m_Data->ShadowComputePipeline)
			m_Data->ShadowComputePipeline = ComputePipeline::Create(pipelineCreateInfo);

		m_Data->ShadowComputeTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width /   1.0f;
			imageSpec.Height = height / 1.0f;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			m_Data->ShadowComputeTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->ShadowComputeDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->ShadowComputeDescriptor[i])
				m_Data->ShadowComputeDescriptor[i] = Material::Create(m_Data->ShadowComputeShader, "ShaderCompute");

			Ref<VulkanMaterial> vulkanDescriptor = m_Data->ShadowComputeDescriptor[i].As<VulkanMaterial>();

			Ref<Image2D> depthBuffer = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetDepthAttachment(i);
			Ref<Image2D> viewPositionTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(2, i);
			Ref<Image2D> normalTexture = m_RenderPassPipeline->GetRenderPassData<VulkanGeometryPass>()->GeometryRenderPass->GetColorAttachment(0, i);

			vulkanDescriptor->Set("u_DepthBuffer", depthBuffer);
			vulkanDescriptor->Set("u_ViewPositionTexture", viewPositionTexture);
			//vulkanDescriptor->Set("u_NormalTexture", normalTexture);
			vulkanDescriptor->Set("u_ShadowDepthTexture", m_Data->ShadowDepthRenderPass->GetDepthAttachment(i));
			vulkanDescriptor->Set("u_ShadowTextureOutput", m_Data->ShadowComputeTexture[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}


	void VulkanShadowPass::ShadowComputeDenoiseInitData(uint32_t width, uint32_t height)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		ComputePipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->ShadowComputeDenoiseShader;
		if (!m_Data->ShadowComputeDenoisePipeline)
			m_Data->ShadowComputeDenoisePipeline = ComputePipeline::Create(pipelineCreateInfo);

		m_Data->ShadowComputeDenoiseTexture.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			ImageSpecification imageSpec{};
			imageSpec.Width = width /   1.0f;
			imageSpec.Height = height / 1.0f;
			imageSpec.Format = ImageFormat::RGBA8;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Sampler.SamplerFilter = ImageFilter::Linear;
			imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;
			m_Data->ShadowComputeDenoiseTexture[i] = Image2D::Create(imageSpec);
		}

		m_Data->ShadowComputeDenoiseDescriptor.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			if (!m_Data->ShadowComputeDenoiseDescriptor[i])
				m_Data->ShadowComputeDenoiseDescriptor[i] = Material::Create(m_Data->ShadowComputeDenoiseShader, "ShaderComputeDenoise");

			Ref<VulkanMaterial> vulkanDescriptor = m_Data->ShadowComputeDenoiseDescriptor[i].As<VulkanMaterial>();

			vulkanDescriptor->Set("i_Texture", m_Data->ShadowComputeTexture[i]);
			vulkanDescriptor->Set("o_Texture", m_Data->ShadowComputeDenoiseTexture[i]);

			vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		}
	}

	void VulkanShadowPass::InitLate()
	{

	}

	void VulkanShadowPass::OnUpdate(const RenderQueue& renderQueue)
	{
		UpdateCascades(renderQueue);

		VulkanRenderer::BeginTimeStampPass("Shadow Pass (Cascades)");
		//ShadowDepthUpdate(renderQueue);
		ShadowDepthUpdateInstancing(renderQueue);
		VulkanRenderer::EndTimeStampPass("Shadow Pass (Cascades)");

		VulkanRenderer::BeginTimeStampPass("Shadow Pass (Compute)");
		ShadowComputeUpdate(renderQueue);
		VulkanRenderer::EndTimeStampPass("Shadow Pass (Compute)");
		//ShadowComputeDenoiseUpdate(renderQueue);
	}



#if 0
	void VulkanShadowPass::ShadowDepthUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<VulkanRenderPass> vulkanRenderPass = m_Data->ShadowDepthRenderPass.As<VulkanRenderPass>();
		Ref<VulkanPipeline> vulkanPipeline = m_Data->ShadowDepthPipeline.As<VulkanPipeline>();


		// Get the needed matricies from the camera
		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectionMatrix * renderQueue.CameraViewMatrix;



		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
		*/
		s_ShadowDepth_MeshIndirectData.clear(); // Reset the vector from the previous frame

		// `Indirect draw commands` offset
		uint64_t indirectCmdsOffset = 0;

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetMeshAsset()->GetSubMeshes();

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
			if (s_ShadowDepth_MeshIndirectData.size() == 0)
			{
				s_ShadowDepth_MeshIndirectData.emplace_back(IndirectMeshData(0, submeshes.size(), i, mesh->GetMaterialCount(), 0));
			}
			else
			{
				uint32_t previousMeshOffset = s_ShadowDepth_MeshIndirectData[i - 1].SubmeshOffset;
				uint32_t previousMeshCount = s_ShadowDepth_MeshIndirectData[i - 1].SubmeshCount;
				uint32_t currentMeshOffset = previousMeshOffset + previousMeshCount;

				uint32_t previousMaterialOffset = s_ShadowDepth_MeshIndirectData[i - 1].MaterialOffset;
				uint32_t previousMaterialCount = s_ShadowDepth_MeshIndirectData[i - 1].MaterialCount;
				uint32_t currentMaterialOffset = previousMaterialOffset + previousMaterialCount;

				s_ShadowDepth_MeshIndirectData.emplace_back(IndirectMeshData(currentMeshOffset, submeshes.size(), i, mesh->GetMaterialCount(), currentMaterialOffset));
			}


		}

		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectVoxelCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);


		// Bind the pipeline
		vulkanPipeline->Bind();



		// Bind the renderpass
		for (uint32_t i = 0; i < 4; i++)
		{

			VkClearValue clearValue{};
			clearValue.depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = vulkanRenderPass->GetVulkanRenderPass();
			renderPassInfo.framebuffer = (VkFramebuffer)vulkanRenderPass->GetFramebuffer(currentFrameIndex)->GetFramebufferHandle();
			renderPassInfo.renderArea.offset = { (int32_t)m_Data->MinResCascade[i].x, (int32_t)m_Data->MinResCascade[i].y };
			renderPassInfo.renderArea.extent = { (uint32_t)m_Data->MaxResCascade[i].x, (uint32_t)m_Data->MaxResCascade[i].y };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;
			vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


			// Set the viewport and scrissors
			VkViewport viewport{};
			viewport.x = m_Data->MinResCascade[i].x;
			viewport.y = m_Data->MinResCascade[i].y;
			viewport.width = (float)m_Data->MaxResCascade[i].x;
			viewport.height = (float)m_Data->MaxResCascade[i].y;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { (int32_t)m_Data->MinResCascade[i].x, (int32_t)m_Data->MinResCascade[i].y };
			scissor.extent = { (uint32_t)m_Data->MaxResCascade[i].x, (uint32_t)m_Data->MaxResCascade[i].y };
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);


			// Sending the indirect draw commands to the command buffer
			for (uint32_t j = 0; j < s_ShadowDepth_MeshIndirectData.size(); j++)
			{
				auto& meshData = s_ShadowDepth_MeshIndirectData[j];
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
				m_PushConstant.VertexBufferBDA = mesh->GetMeshAsset()->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
				m_PushConstant.CascadeIndex = 0;
				m_PushConstant.ViewProjectionMatrix = m_Data->CascadeViewProjMatrix[i];
				m_PushConstant.IsAnimated = static_cast<uint32_t>(mesh->IsAnimated());

				if (mesh->IsAnimated())
					m_PushConstant.BoneInformationBDA = mesh->GetBoneUniformBuffer(currentFrameIndex).As<VulkanUniformBuffer>()->GetVulkanBufferAddress();
				else
					m_PushConstant.BoneInformationBDA = 0;

				vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_PushConstant);

				uint32_t submeshCount = meshData.SubmeshCount;
				uint32_t offset = s_ShadowDepth_MeshIndirectData[j].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
				vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));
			}

			vulkanRenderPass->Unbind();
		}
	}
#endif

	struct MeshInstanceListShadowPass // This is reponsible for grouping all the mesh instances into one array
	{
		Mesh* Mesh;
		glm::mat4 Transform;
	};

	// This is reponsible for grouping all the mesh instances into one array
	// Declaring it here to not allocate a new hashmap every frame
	static HashMap<AssetHandle, Vector<MeshInstanceListShadowPass>> s_GroupedMeshesCached;
	static Vector<NewIndirectMeshData> s_ShadowDepthMeshIndirectData; // Made a static variable, to not allocate new data everyframe

	void VulkanShadowPass::ShadowDepthUpdateInstancing(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<VulkanRenderPass> vulkanRenderPass = m_Data->ShadowDepthRenderPass.As<VulkanRenderPass>();
		Ref<VulkanPipeline> vulkanPipeline = m_Data->ShadowDepthPipeline.As<VulkanPipeline>();

		// Get the needed matricies from the camera
		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectionMatrix * renderQueue.CameraViewMatrix;

		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
			(TODO: This explanation is outdated, now we are using `instanced indirect multidraw rendering` :D)
		*/
		s_ShadowDepthMeshIndirectData.clear();
		s_GroupedMeshesCached.clear();

		// Allocate all the neccesary array buffers before, so we won't waste cpus cycles on reallocating memory
		for (auto& [meshAssetUUID, instanceCount] : renderQueue.m_MeshInstanceCount)
		{
			s_GroupedMeshesCached[meshAssetUUID].reserve(instanceCount);
		}
		s_ShadowDepthMeshIndirectData.reserve(s_GroupedMeshesCached.size());


		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			s_GroupedMeshesCached[mesh->GetMeshAsset()->Handle].push_back({ mesh.Raw(), renderQueue.m_Data[i].Transform });
		}

		// `Indirect draw commands` offset
		uint64_t indirectCmdsOffset = 0;

		// `Instance data` offset.
		uint64_t materialDataOffset = 0;

		// `Instance data` offset.
		uint64_t instanceVertexOffset = 0;
		
		for (auto& [handle, groupedMeshes] : s_GroupedMeshesCached)
		{
			NewIndirectMeshData* currentIndirectMeshData;
			NewIndirectMeshData* lastIndirectMeshData = nullptr;

			// If we are submitting the first mesh, we don't need any offset
			currentIndirectMeshData = &s_ShadowDepthMeshIndirectData.emplace_back();
			
			// We are checking `.size() > 1` and also `.size() - 2` because we've already pushed_back and element before, so we should go 2 elements behind instead of one
			if (s_ShadowDepthMeshIndirectData.size() > 1)
				lastIndirectMeshData = &s_ShadowDepthMeshIndirectData[s_ShadowDepthMeshIndirectData.size() - 2];


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
			if (s_ShadowDepthMeshIndirectData.size() > 1)
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


					// Adding the neccesary Matricies for the shader
					meshInstancedVertexBuffer.ModelSpaceMatrix = modelMatrix;
					meshInstancedVertexBuffer.WorldSpaceMatrix = viewProjectionMatrix * modelMatrix;
					/////////////////////////////////////////////////////

					// Other stuff
					if (meshInstance.Mesh->IsAnimated())
						meshInstancedVertexBuffer.BoneInformationBDA = meshInstance.Mesh->GetBoneUniformBuffer(currentFrameIndex).As<VulkanUniformBuffer>()->GetVulkanBufferAddress();
					else
						meshInstancedVertexBuffer.BoneInformationBDA = 0;
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

				m_Data->IndirectShadowCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
			}
		}
		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectShadowCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectShadowCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);

		// Global Instanced Vertex Buffer data
		auto vulkanInstancedVertexBuffer = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* instancedVertexBufferPointer = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanInstancedVertexBuffer->SetData(instanceVertexOffset, instancedVertexBufferPointer);



		// Bind the pipeline
		vulkanPipeline->Bind();

		// Bind the renderpass
		for (uint32_t i = 0; i < 4; i++)
		{

			VkClearValue clearValue{};
			clearValue.depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = vulkanRenderPass->GetVulkanRenderPass();
			renderPassInfo.framebuffer = (VkFramebuffer)vulkanRenderPass->GetFramebuffer(currentFrameIndex)->GetFramebufferHandle();
			renderPassInfo.renderArea.offset = { (int32_t)m_Data->MinResCascade[i].x, (int32_t)m_Data->MinResCascade[i].y };
			renderPassInfo.renderArea.extent = { (uint32_t)m_Data->MaxResCascade[i].x, (uint32_t)m_Data->MaxResCascade[i].y };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;
			vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);


			// Set the viewport and scrissors
			VkViewport viewport{};
			viewport.x = m_Data->MinResCascade[i].x;
			viewport.y = m_Data->MinResCascade[i].y;
			viewport.width = (float)m_Data->MaxResCascade[i].x;
			viewport.height = (float)m_Data->MaxResCascade[i].y;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.offset = { (int32_t)m_Data->MinResCascade[i].x, (int32_t)m_Data->MinResCascade[i].y };
			scissor.extent = { (uint32_t)m_Data->MaxResCascade[i].x, (uint32_t)m_Data->MaxResCascade[i].y };
			vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

			// Binding the global instanced vertex buffer only once
			auto vulkanVertexBufferInstanced = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
			VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
			VkDeviceSize deviceSize[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);


			// Sending the indirect draw commands to the command buffer
			for (uint32_t j = 0; j < s_ShadowDepthMeshIndirectData.size(); j++)
			{
				auto& indirectPerMeshData = s_ShadowDepthMeshIndirectData[j];

				// Get the mesh
				const AssetMetadata& assetMetadata = AssetManager::GetMetadata(indirectPerMeshData.MeshAssetHandle);
				Ref<MeshAsset> meshAsset = AssetManager::GetAsset<MeshAsset>(assetMetadata.FilePath.string());

				// Bind the index buffer
				meshAsset->GetIndexBuffer()->Bind();


				// Set the transform matrix and model matrix of the submesh into a constant buffer
				m_PushConstant.VertexBufferBDA = meshAsset->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
				m_PushConstant.ViewProjectionMatrix = m_Data->CascadeViewProjMatrix[i];
				m_PushConstant.IsAnimated = static_cast<uint32_t>(meshAsset->IsAnimated());

				vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_PushConstant);

				uint32_t submeshCount = indirectPerMeshData.SubmeshCount;
				uint32_t offset = indirectPerMeshData.CmdOffset * sizeof(VkDrawIndexedIndirectCommand);
				vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));
			}

			vulkanRenderPass->Unbind();
		}
	}

	void VulkanShadowPass::ShadowComputeUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSpec = Renderer::GetRendererSettings();

		auto vulkanPipeline = m_Data->ShadowComputePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->ShadowComputeDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix0", m_Data->CascadeViewProjMatrix[0]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix1", m_Data->CascadeViewProjMatrix[1]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix2", m_Data->CascadeViewProjMatrix[2]);
		vulkanDescriptor->Set("DirectionaLightData.LightViewProjMatrix3", m_Data->CascadeViewProjMatrix[3]);
		vulkanDescriptor->Set("DirectionaLightData.DirectionalLightDir", renderQueue.m_LightData.DirLight.Direction);
		vulkanDescriptor->Set("DirectionaLightData.DirectionLightSize", renderQueue.m_LightData.DirLight.Specification.Size);
		vulkanDescriptor->Set("DirectionaLightData.FadeCascades", rendererSpec.ShadowPass.FadeCascades);
		vulkanDescriptor->Set("DirectionaLightData.CascadesFadeFactor", rendererSpec.ShadowPass.CascadesFadeFactor);
		vulkanDescriptor->Set("DirectionaLightData.CascadeDebug", rendererSpec.ShadowPass.m_ShowCascadesDebug);
		vulkanDescriptor->Set("DirectionaLightData.UsePCSS", rendererSpec.ShadowPass.UsePCSS);

		vulkanDescriptor->Bind(cmdBuf, m_Data->ShadowComputePipeline);

		m_PushConstantShadowComputeData.CascadeDepthSplit = m_Data->CascadeDepthSplit;
		m_PushConstantShadowComputeData.InvViewProjection = glm::inverse(renderQueue.m_Camera->GetViewProjectionVK());
		m_PushConstantShadowComputeData.NearCameraClip = renderQueue.m_Camera->GetNearClip();
		m_PushConstantShadowComputeData.FarCameraClip = renderQueue.m_Camera->GetFarClip();

		vulkanPipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_PushConstantShadowComputeData);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = static_cast<uint32_t>(std::ceil((width /  1.0f) / 8.0f));
		uint32_t groupY = static_cast<uint32_t>(std::ceil((height / 1.0f) / 8.0f));
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Barrier
		Ref<VulkanImage2D> vulkanTexture = m_Data->ShadowComputeTexture[currentFrameIndex].As<VulkanImage2D>();
		vulkanTexture->TransitionLayout(cmdBuf, vulkanTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	void VulkanShadowPass::ShadowComputeDenoiseUpdate(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		RendererSettings& rendererSpec = Renderer::GetRendererSettings();

		auto vulkanPipeline = m_Data->ShadowComputeDenoisePipeline.As<VulkanComputePipeline>();
		auto vulkanDescriptor = m_Data->ShadowComputeDenoiseDescriptor[currentFrameIndex].As<VulkanMaterial>();

		vulkanDescriptor->Bind(cmdBuf, m_Data->ShadowComputeDenoisePipeline);

		float width = renderQueue.ViewPortWidth;
		float height = renderQueue.ViewPortHeight;

		uint32_t groupX = static_cast<uint32_t>(std::ceil((width /  1.0f) / 32.0f));
		uint32_t groupY = static_cast<uint32_t>(std::ceil((height / 1.0f) / 32.0f));
		vulkanPipeline->Dispatch(cmdBuf, groupX, groupY, 1);

		// Barrier
		Ref<VulkanImage2D> vulkanTexture = m_Data->ShadowComputeDenoiseTexture[currentFrameIndex].As<VulkanImage2D>();
		vulkanTexture->TransitionLayout(cmdBuf, vulkanTexture->GetVulkanImageLayout(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}

	void VulkanShadowPass::UpdateCascades(const RenderQueue& renderQueue)
	{
		RendererSettings& rendererSettings = Renderer::GetRendererSettings();

		//const int32_t CASCADE_COUNT = Renderer::GetRendererSettings().ShadowPass.CascadeCount;
		int32_t CASCADE_COUNT = Renderer::GetRendererSettings().ShadowPass.CascadeCount;

		Vector<float> cascadeSplits(CASCADE_COUNT);
		float shadowTextureRes = Renderer::GetRendererConfig().ShadowTextureResolution;

		float nearClip = renderQueue.m_Camera->GetNearClip();
		float farClip = renderQueue.m_Camera->GetFarClip();
		//float nearClip = rendererSettings.ShadowPass.CameraNearClip;
		//float farClip = rendererSettings.ShadowPass.CameraFarClip;

		float clipRange = farClip - nearClip;

		float minZ = nearClip;
		float maxZ = nearClip + clipRange;

		float range = maxZ - minZ;
		float ratio = maxZ / minZ;

		// Calculate split depths based on view camera frustum
		// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (uint32_t i = 0; i < CASCADE_COUNT; i++)
		{
			float p = (i + 1) / static_cast<float>(CASCADE_COUNT);
			float log = minZ * std::pow(ratio, p);
			float uniform = minZ + range * p;
			float d = rendererSettings.ShadowPass.CascadeSplitLambda * (log - uniform) + uniform;
			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		//cascadeSplits[3] = 0.3f;


		// Calculate orthographic projection matrix for each cascade
		float lastSplitDist = 0.0;
		for (uint32_t i = 0; i < CASCADE_COUNT; i++)
		{
			float splitDist = cascadeSplits[i];

			glm::vec4 frustumCorners[8] = {
                //Near face
                {1.0f, 1.0f, -1.0f, 1.0f},
                {-1.0f, 1.0f, -1.0f, 1.0f},
                {1.0f, -1.0f, -1.0f, 1.0f},
                {-1.0f, -1.0f, -1.0f, 1.0f},

                //Far face
                {1.0f, 1.0f, 1.0f, 1.0f},
                {-1.0f, 1.0f, 1.0f, 1.0f},
                {1.0f, -1.0f, 1.0f, 1.0f},
                {-1.0f, -1.0f, 1.0f, 1.0f},
            };

			// Project frustum corners into world space
			glm::mat4 inverseCamViewProjection = glm::inverse(renderQueue.m_Camera->GetViewProjection());
			for (glm::vec4& frustumCorner : frustumCorners)
			{
				glm::vec4 invCorner = inverseCamViewProjection * frustumCorner;
				frustumCorner = invCorner / invCorner.w;
			}

			for (uint32_t i = 0; i < 4; i++)
			{
				glm::vec4 dist = frustumCorners[i + 4] - frustumCorners[i];
				frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
				frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
			}

			// Get frustum center
			glm::vec3 frustumCenter = glm::vec3(0.0f);
			for (glm::vec4& frustumCorner : frustumCorners)
			{
				frustumCenter += glm::vec3(frustumCorner);
			}
			frustumCenter /= 8.0f;


			// Get the minimum and maximum extents
			float radius = 0.0f;
			for (glm::vec4& frustumCorner : frustumCorners)
			{
				float distance = glm::length(glm::vec3(frustumCorner) - frustumCenter);
				radius = glm::max(radius, distance);
			}
			radius = std::ceil(radius);

			glm::vec3 maxExtents = glm::vec3(radius);
			glm::vec3 minExtents = -maxExtents;

			glm::vec3 lightDir = glm::normalize(-renderQueue.m_LightData.DirLight.Direction);
			glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));

			glm::mat4 lightOrthoMatrix = glm::ortho(
				minExtents.x, maxExtents.x,
				minExtents.y, maxExtents.y,
				0.0f + rendererSettings.ShadowPass.CascadeNearPlaneOffset, (maxExtents.z - minExtents.z) + rendererSettings.ShadowPass.CascadeFarPlaneOffset
			);
			//glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

			// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			shadowOrigin = shadowMatrix * shadowOrigin;
			float storedW = shadowOrigin.w;
			shadowOrigin = shadowOrigin * static_cast<float>(shadowTextureRes) / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / static_cast<float>(shadowTextureRes);
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;
			glm::mat4 shadowProj = lightOrthoMatrix;
			shadowProj[3] += roundOffset;
			lightOrthoMatrix = shadowProj;
#if 0
			glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
			const float ShadowMapResolution = shadowTextureRes;
			glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
			glm::vec4 roundedOrigin = glm::round(shadowOrigin);
			glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
			roundOffset = roundOffset * 2.0f / ShadowMapResolution;
			roundOffset.z = 0.0f;
			roundOffset.w = 0.0f;
			lightOrthoMatrix[3] += roundOffset;
#endif



			m_Data->CascadeDepthSplit[i] = (nearClip + splitDist * clipRange) * -1.0f;
			m_Data->CascadeViewProjMatrix[i] = lightOrthoMatrix * lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}
	}

	void VulkanShadowPass::CalculateCascadeOffsets()
	{
		uint32_t shadowTextureRes = Renderer::GetRendererConfig().ShadowTextureResolution;

		m_Data->MinResCascade[0] = { 0.0f            , 0.0f };
		m_Data->MaxResCascade[0] = { shadowTextureRes, shadowTextureRes };

		m_Data->MinResCascade[1] = { shadowTextureRes , 0 };
		m_Data->MaxResCascade[1] = { shadowTextureRes , shadowTextureRes };

		m_Data->MinResCascade[2] = { 0               , shadowTextureRes };
		m_Data->MaxResCascade[2] = { shadowTextureRes, shadowTextureRes };

		m_Data->MinResCascade[3] = { shadowTextureRes, shadowTextureRes };
		m_Data->MaxResCascade[3] = { shadowTextureRes, shadowTextureRes };
	}

	void VulkanShadowPass::OnRenderDebug()
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		uint32_t shadowTextureRes = Renderer::GetRendererConfig().ShadowTextureResolution;
		RendererSettings& rendererSpec = Renderer::GetRendererSettings();

		if (ImGui::CollapsingHeader("Shadow Depth Pass"))
		{
			ImGuiLayer* imguiLayer = Application::Get().GetImGuiLayer();

			ImGui::DragFloat("Cascade Split Factor", &rendererSpec.ShadowPass.CascadeSplitLambda, 0.005f, 0.1f, 1.5f);
			//ImGui::DragFloat("Camera FarClip", &rendererSpec.ShadowPass.CameraFarClip, 10.0f, 0.0f, 10000.0f);
			//ImGui::DragFloat("Camera NearClip", &rendererSpec.ShadowPass.CameraNearClip, 0.05f, 0.05f, 1.0);
			ImGui::DragFloat("Cascade NearPlane Offset", &rendererSpec.ShadowPass.CascadeNearPlaneOffset, 0.5f);
			ImGui::DragFloat("Cascade FarPlane Offset", &rendererSpec.ShadowPass.CascadeFarPlaneOffset, 0.5f);
			ImGui::DragFloat("Fade Cascades Factor", &rendererSpec.ShadowPass.CascadesFadeFactor, 0.1f, 0.00f, 10.0f);
			ImGui::SliderInt("Use PCSS", &rendererSpec.ShadowPass.UsePCSS, 0, 1);
			ImGui::SliderInt("Fade Cascades", &rendererSpec.ShadowPass.FadeCascades, 0, 1);
			ImGui::SliderInt("Show Cascades", &rendererSpec.ShadowPass.m_ShowCascadesDebug, 0, 1);

			auto shadowDepthTexture = m_Data->ShadowDepthRenderPass->GetDepthAttachment(currentFrameIndex);
			imguiLayer->RenderTexture(shadowDepthTexture, 256, 256);
		}
	}

	void VulkanShadowPass::OnResize(uint32_t width, uint32_t height)
	{
		ShadowComputeInitData(width, height);
		ShadowComputeDenoiseInitData(width, height);
	}

	void VulkanShadowPass::OnResizeLate(uint32_t width, uint32_t height)
	{

	}

	void VulkanShadowPass::ShutDown()
	{
		delete m_Data;
	}

}
