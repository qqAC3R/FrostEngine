#include "frostpch.h"
#include "VulkanGeometryPass.h"

#include "Frost/Utils/Timer.h"

#include "Frost/Platform/Vulkan/VulkanFramebuffer.h"
#include "Frost/Platform/Vulkan/VulkanRenderPass.h"
#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanPipeline.h"
#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/VulkanTexture.h"
#include "Frost/Platform/Vulkan/VulkanImage.h"
#include "Frost/Platform/Vulkan/VulkanMaterial.h"
#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanUniformBuffer.h"

#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"

#include "Frost/Platform/Vulkan/SceneRenderPasses/VulkanPostFXPass.h"

#include "Frost/Asset/AssetManager.h"

namespace Frost
{

	VulkanGeometryPass::VulkanGeometryPass()
		: m_Name("GeometryPass")
	{
	}

	VulkanGeometryPass::~VulkanGeometryPass()
	{
	}

	static RenderPassSpecification s_RenderPassSpec;

	void VulkanGeometryPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->GeometryShader = Renderer::GetShaderLibrary()->Get("GeometryPassIndirectInstancedBindless");
		m_Data->LateCullShader = Renderer::GetShaderLibrary()->Get("OcclusionCulling_V2");


		GeometryDataInit(1600, 900);

		Renderer::SubmitImageToOutputImageMap("Albedo", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->GeometryRenderPass->GetColorAttachment(2, currentFrameIndex);
		});
	}


	void VulkanGeometryPass::InitLate()
	{
		OcclusionCullDataInit(1600, 900);
	}

	/// Geometry pass initialization
	void VulkanGeometryPass::GeometryDataInit(uint32_t width, uint32_t height)
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		s_RenderPassSpec =
		{
			width, height, framesInFlight,
			{
				// Position Attachment // Attachment 0
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Normals Attachment // Attachment 1
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Albedo Attachment // Attachment 2
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// View-space Position Attachment // Attachment 3
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Id Entity Attachment // Attachment 4
				{
					FramebufferTextureFormat::R32I, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				}
			}
		};
		m_Data->GeometryRenderPass = RenderPass::Create(s_RenderPassSpec);


		// Pipeline creations

		BufferLayout bufferLayout = {
			{ "a_ModelSpaceMatrix",           ShaderDataType::Mat4   },
			{ "a_WorldSpaceMatrix",           ShaderDataType::Mat4   },
			{ "a_MaterialIndexGlobalOffset",  ShaderDataType::UInt   },
			{ "a_EntityID",                   ShaderDataType::UInt   },
			{ "a_BoneInformationBDA",         ShaderDataType::UInt64 }
		};
		bufferLayout.m_InputType = InputType::Instanced;

		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->GeometryShader;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.UseStencil = false;
		pipelineCreateInfo.Cull = CullMode::Back;
		pipelineCreateInfo.RenderPass = m_Data->GeometryRenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		if(!m_Data->GeometryPipeline)
			m_Data->GeometryPipeline = Pipeline::Create(pipelineCreateInfo);


		/// Geometry pass descriptors (from PBR Shader)
		m_Data->GeometryDescriptor.resize(framesInFlight);
		for (auto& material : m_Data->GeometryDescriptor)
		{
			if(!material) material = Material::Create(m_Data->GeometryShader, "GeometryPassIndirectInstancedBindless");
		}

		if (m_Data->IndirectCmdBuffer.empty())
		{
			/// Indirect drawing buffer
			m_Data->IndirectCmdBuffer.resize(framesInFlight);
			for (auto& indirectCmdBuffer : m_Data->IndirectCmdBuffer)
			{
				// Allocating a heap block
				indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes, { BufferUsage::Storage, BufferUsage::Indirect });

				indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes);
			}

			/// Instance data storage buffer
			m_Data->MaterialSpecs.resize(framesInFlight);
			for (uint32_t i = 0; i < m_Data->MaterialSpecs.size(); i++)
			{
				auto& instanceSpec = m_Data->MaterialSpecs[i];

				// Allocating a heap block
				instanceSpec.DeviceBuffer = BufferDevice::Create(sizeof(MaterialData) * maxCountMeshes, { BufferUsage::Storage });
				instanceSpec.HostBuffer.Allocate(sizeof(MaterialData) * maxCountMeshes);

				// Setting the storage buffer into the descriptor
				m_Data->GeometryDescriptor[i]->Set("u_MaterialUniform", instanceSpec.DeviceBuffer);
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
		}

	}

	void VulkanGeometryPass::OcclusionCullDataInit(uint32_t width, uint32_t height)
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		m_Data->ComputeShaderPushConstant.DepthPyramidSize.x = width;
		m_Data->ComputeShaderPushConstant.DepthPyramidSize.y = height;

		//////////////////////////////////////////////
		// LATE CULLING //////////////////////////////
		//////////////////////////////////////////////
		{
			if (!m_Data->LateCullPipeline)
			{
				ComputePipeline::CreateInfo computePipelineCreateInfo{};
				computePipelineCreateInfo.Shader = m_Data->LateCullShader;
				m_Data->LateCullPipeline = ComputePipeline::Create(computePipelineCreateInfo);

				m_Data->DebugDeviceBuffer = BufferDevice::Create(sizeof(glm::mat4) * 1024, { BufferUsage::Storage });

				m_Data->MeshSpecs.DeviceBuffer = BufferDevice::Create(sizeof(MeshData_OC) * maxCountMeshes, { BufferUsage::Storage });
				m_Data->MeshSpecs.HostBuffer.Allocate(sizeof(MeshData_OC) * maxCountMeshes);
			}

			m_Data->LateCullDescriptor.resize(framesInFlight);
			for (uint32_t i = 0; i < m_Data->LateCullDescriptor.size(); i++)
			{
				auto& computeDescriptor = m_Data->LateCullDescriptor[i];

				int32_t previousFrameIndex = (int32_t)i - 1;
				if (previousFrameIndex < 0)
					previousFrameIndex = Renderer::GetRendererConfig().FramesInFlight - 1;

				auto lastFrameDepthPyramid = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->DepthPyramid[previousFrameIndex];

				if(!computeDescriptor)
					computeDescriptor = Material::Create(m_Data->LateCullShader, "OcclusionCulling");

				computeDescriptor->Set("DepthPyramid", lastFrameDepthPyramid);
				computeDescriptor->Set("MeshSpecs", m_Data->MeshSpecs.DeviceBuffer);
				computeDescriptor->Set("DrawCommands", m_Data->IndirectCmdBuffer[i].DeviceBuffer);
				computeDescriptor->Set("DebugBuffer", m_Data->DebugDeviceBuffer);


				auto& computeVulkanDescriptor = m_Data->LateCullDescriptor[i].As<VulkanMaterial>();
				computeVulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
			}
		}
	}


	void VulkanGeometryPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;


		VulkanRenderer::BeginTimeStampPass("Geometry Pass");
		//GeometryPrepareIndirectData(renderQueue);
		GeometryPrepareIndirectDataWithInstacing(renderQueue);
		GeometryUpdateWithInstancing(renderQueue);
		VulkanRenderer::EndTimeStampPass("Geometry Pass");
	}



	struct MeshInstanceListGeometryPass // This is reponsible for grouping all the mesh instances into one array
	{
		Mesh* Mesh; // Getting it as a raw pointer to intefere with the reference count
		glm::mat4 Transform;
		uint32_t MeshIndex;
		uint32_t EntityID;
	};

	// This is reponsible for grouping all the mesh instances into one array
	// Declaring it here to not allocate a new hashmap every frame
	static HashMap<AssetHandle, Vector<MeshInstanceListGeometryPass>> s_GroupedMeshesCached;
	static Vector<NewIndirectMeshData> s_GeometryMeshIndirectData; // Made a static variable, to not allocate new data everyframe

	void VulkanGeometryPass::GeometryPrepareIndirectDataWithInstacing(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

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
		s_GeometryMeshIndirectData.clear();
		s_GroupedMeshesCached.clear();

		//for (auto& [meshAssetUUID, instanceCount] : renderQueue.m_MeshInstanceCount)
		//{
		//	s_GroupedMeshesCached[meshAssetUUID].reserve(instanceCount);
		//}


		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;

			s_GroupedMeshesCached[mesh->GetMeshAsset()->Handle].push_back({ mesh.Raw(), renderQueue.m_Data[i].Transform, i, renderQueue.m_Data[i].EntityID});
		}

		//FROST_CORE_INFO(renderQueue.m_Data.size());


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
			if (s_GeometryMeshIndirectData.size() > 0)
				lastIndirectMeshData = &s_GeometryMeshIndirectData[s_GeometryMeshIndirectData.size() - 1];

			// If we are submitting the first mesh, we don't need any offset
			currentIndirectMeshData = &s_GeometryMeshIndirectData.emplace_back();

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
			if (s_GeometryMeshIndirectData.size() > 1)
			{
				currentIndirectMeshData->MaterialOffset = lastIndirectMeshData->MaterialOffset + (lastIndirectMeshData->MaterialCount * lastIndirectMeshData->InstanceCount);
				currentIndirectMeshData->TotalMeshOffset = lastIndirectMeshData->TotalMeshOffset + (lastIndirectMeshData->SubmeshCount * lastIndirectMeshData->InstanceCount);
			}

			// Set up the materials firstly (per instance, per material)
			for (auto& meshInstance : groupedMeshes)
			{
				for (uint32_t k = 0; k < currentIndirectMeshData->MaterialCount; k++)
				{
					// Setting up the material data into a storage buffer
					Ref<DataStorage> materialData = meshInstance.Mesh->GetMaterialAsset(k)->GetMaterialInternalData();

					m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Write(materialData->GetBufferData(), sizeof(MaterialData), materialDataOffset);

					materialDataOffset += sizeof(MaterialData);
				}
			}

			// Set up the instanced vertex buffer (per submesh, per instance)
			for(uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); submeshIndex++)
			{
				MeshInstancedVertexBuffer meshInstancedVertexBuffer{};
				uint32_t meshInstanceNr = 0;
				for (auto& meshInstance : groupedMeshes)
				{
					//glm::mat4 modelMatrix = meshInstance.Transform * submeshes[submeshIndex].Transform;

					 // Using skeletal (dynamic) submesh transforms, instead of the static ones which are found in the mesh asset
					glm::mat4 modelMatrix = meshInstance.Transform * meshInstance.Mesh->GetSkeletalSubmeshes()[submeshIndex].Transform;


					// Adding the neccesary Matricies for the shader
					meshInstancedVertexBuffer.ModelSpaceMatrix = modelMatrix;
					meshInstancedVertexBuffer.WorldSpaceMatrix = viewProjectionMatrix * modelMatrix;
					/////////////////////////////////////////////////////
					
					// Doing `MaterialOffset` because we are indicating it to the whole material buffer (so the index should be global)
					meshInstancedVertexBuffer.MaterialIndexOffset = currentIndirectMeshData->MaterialOffset + (meshInstanceNr * currentIndirectMeshData->MaterialCount);
					/////////////////////////////////////////////////////

					// Other stuff
					meshInstancedVertexBuffer.EntityID = meshInstance.EntityID;

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

				m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
			}
		}
		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);

		// Material Instance data
		auto vulkanInstanceDataBuffer = m_Data->MaterialSpecs[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* instanceDataPointer = m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Data;
		vulkanInstanceDataBuffer->SetData(materialDataOffset, instanceDataPointer);

		// Global Instanced Vertex Buffer data
		auto vulkanInstancedVertexBuffer = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* instancedVertexBufferPointer = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanInstancedVertexBuffer->SetData(instanceVertexOffset, instancedVertexBufferPointer);
	}

	void VulkanGeometryPass::GeometryUpdateWithInstancing(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->GeometryRenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->GeometryPipeline.As<VulkanPipeline>();
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();

		// Bind the pipeline and renderpass
		m_Data->GeometryRenderPass->Bind();
		m_Data->GeometryPipeline->Bind();

		// Set the viewport and scrissors
		VkViewport viewport{};
		viewport.width = (float)framebuffer->GetSpecification().Width;
		viewport.height = (float)framebuffer->GetSpecification().Height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { framebuffer->GetSpecification().Width, framebuffer->GetSpecification().Height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);


		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->GeometryPipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->GeometryDescriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);



		// Binding the global instanced vertex buffer only once
		auto vulkanVertexBufferInstanced = m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
		VkDeviceSize deviceSize[1] = { 0 };
		vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);

		// Sending the indirect draw commands to the command buffer
		for (uint32_t i = 0; i < s_GeometryMeshIndirectData.size(); i++)
		{
			auto& indirectPerMeshData = s_GeometryMeshIndirectData[i];

			// Get the mesh
			const AssetMetadata& assetMetadata = AssetManager::GetMetadata(indirectPerMeshData.MeshAssetHandle);
			Ref<MeshAsset> meshAsset = AssetManager::GetAsset<MeshAsset>(assetMetadata.FilePath.string());

			// Bind the index buffer
			meshAsset->GetIndexBuffer()->Bind();

			m_GeometryPushConstant.VertexBufferBDA = meshAsset->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			m_GeometryPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
			m_GeometryPushConstant.IsAnimated = static_cast<uint32_t>(meshAsset->IsAnimated());

			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_GeometryPushConstant);

			uint32_t submeshCount = indirectPerMeshData.SubmeshCount;
			uint32_t offset = indirectPerMeshData.CmdOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));
		}
		// End the renderpass
		m_Data->GeometryRenderPass->Unbind();
	}

#if 0
	void VulkanGeometryPass::GeometryPrepareIndirectData(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		// Get the needed matricies from the camera
		glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectionMatrix * renderQueue.CameraViewMatrix;

		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
		*/
		s_Geometry_MeshIndirectData.clear();

		// `Indirect draw commands` offset
		uint64_t indirectCmdsOffset = 0;

		// Data for occlusion culling shader
		MeshData_OC meshData{};
		uint32_t meshDataOffset = 0;

		// `Instance data` offset.
		uint32_t materialDataOffset = 0;

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetMeshAsset()->GetSubMeshes();

			// Count how many meshes were submitted (for calculating offsets)
			uint32_t submittedSubmeshes = 0;

			mesh->UpdateInstancedVertexBuffer(renderQueue.m_Data[i].Transform, viewProjectionMatrix, currentFrameIndex);

			// Set commands for the submeshes
			for (uint32_t k = 0; k < submeshes.size(); k++)
			{
				const Submesh& submesh = submeshes[k];

				glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;

				// TODO: Fix frustum culling
#if 0
				//if (!submesh.BoundingBox.IsOnFrustum(renderQueue.m_Camera.GetFrustum(), modelMatrix))
				//	continue;
#endif

				// Submit the submesh into the cpu buffer
				VkDrawIndexedIndirectCommand indirectCmdBuf{};
				indirectCmdBuf.firstIndex = submesh.BaseIndex;
				indirectCmdBuf.firstInstance = 0;
				indirectCmdBuf.indexCount = submesh.IndexCount;
				indirectCmdBuf.instanceCount = 1;
				indirectCmdBuf.vertexOffset = submesh.BaseVertex;
				m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);

				// Setting up `Mesh data` for the occlusion culling compute shader
				meshData.Transform = modelMatrix;
				meshData.AABB_Min = glm::vec4(submesh.BoundingBox.Min, 1.0f);
				meshData.AABB_Max = glm::vec4(submesh.BoundingBox.Max, 1.0f);
				m_Data->MeshSpecs.HostBuffer.Write((void*)&meshData, sizeof(MeshData_OC), meshDataOffset);

				// Adding up the offset
				meshDataOffset += sizeof(MeshData_OC);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
				submittedSubmeshes += 1;
			}


			for (uint32_t k = 0; k < mesh->GetMaterialCount(); k++)
			{
				// Setting up the material data into a storage buffer
				Ref<DataStorage> materialData = mesh->GetMaterialAsset(k)->GetMaterialInternalData();

				DataStorage* dataStorage = materialData.Raw();
				m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Write(dataStorage->GetBufferData(), sizeof(MaterialData), materialDataOffset);

				materialDataOffset += sizeof(MaterialData);
			}

			// If we are submitting the first mesh, we don't need any offset
			if (s_Geometry_MeshIndirectData.size() == 0)
			{
				s_Geometry_MeshIndirectData.emplace_back(IndirectMeshData(0, submeshes.size(), i, mesh->GetMaterialCount(), 0, mesh->GetMeshAsset()->Handle));
			}
			else
			{
				uint32_t previousMeshOffset = s_Geometry_MeshIndirectData[i - 1].SubmeshOffset;
				uint32_t previousMeshCount = s_Geometry_MeshIndirectData[i - 1].SubmeshCount;
				uint32_t currentMeshOffset = previousMeshOffset + previousMeshCount;

				uint32_t previousMaterialOffset = s_Geometry_MeshIndirectData[i - 1].MaterialOffset;
				uint32_t previousMaterialCount = s_Geometry_MeshIndirectData[i - 1].MaterialCount;
				uint32_t currentMaterialOffset = previousMaterialOffset + previousMaterialCount;

				s_Geometry_MeshIndirectData.emplace_back(IndirectMeshData(
					currentMeshOffset,
					submeshes.size(),
					i,
					mesh->GetMaterialCount(),
					currentMaterialOffset,
					mesh->GetMeshAsset()->Handle
				));

			}
		}

		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);

		// Instance data
		auto vulkanInstanceDataBuffer = m_Data->MaterialSpecs[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* instanceDataPointer = m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Data;
		vulkanInstanceDataBuffer->SetData(materialDataOffset, instanceDataPointer);

#if  0
		// Mesh data (occlusion culling)
		auto vulkanMeshDataBuffer = m_Data->MeshSpecs.DeviceBuffer.As<VulkanBufferDevice>();
		void* meshDataPointer = m_Data->MeshSpecs.HostBuffer.Data;
		vulkanMeshDataBuffer->SetData(meshDataOffset, meshDataPointer);

		OcclusionCullUpdate(renderQueue, indirectCmdsOffset);
#endif

	}
#endif

#if 0
	void VulkanGeometryPass::GeometryUpdate(const RenderQueue& renderQueue, const Vector<IndirectMeshData>& indirectMeshData)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->GeometryRenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->GeometryPipeline.As<VulkanPipeline>();
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();

		// Bind the pipeline and renderpass
		m_Data->GeometryRenderPass->Bind();
		m_Data->GeometryPipeline->Bind();

		// Set the viewport and scrissors
		VkViewport viewport{};
		viewport.width = (float)framebuffer->GetSpecification().Width;
		viewport.height = (float)framebuffer->GetSpecification().Height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { framebuffer->GetSpecification().Width, framebuffer->GetSpecification().Height };
		scissor.offset = { 0, 0 };
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);


		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->GeometryPipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->GeometryDescriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);

		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
		//m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);




		// Sending the indirect draw commands to the command buffer
		for (uint32_t i = 0; i < indirectMeshData.size(); i++)
		{
			auto& meshData = indirectMeshData[i];
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
			
			 // This information should be excluded from the push constants and put in the global vertex instaced buffer
			m_GeometryPushConstant.MaterialIndex = indirectMeshData[i].MaterialOffset;
			m_GeometryPushConstant.EntityID = renderQueue.m_Data[meshIndex].EntityID;

			m_GeometryPushConstant.VertexBufferBDA = mesh->GetMeshAsset()->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			m_GeometryPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
			m_GeometryPushConstant.IsAnimated = static_cast<uint32_t>(mesh->IsAnimated());

			if(mesh->IsAnimated())
				m_GeometryPushConstant.BoneInformationBDA = mesh->GetBoneUniformBuffer(currentFrameIndex).As<VulkanUniformBuffer>()->GetVulkanBufferAddress();
			else
				m_GeometryPushConstant.BoneInformationBDA = 0;

			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_GeometryPushConstant);

			uint32_t submeshCount = meshData.SubmeshCount;
			uint32_t offset = indirectMeshData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));
		}
		// End the renderpass
		m_Data->GeometryRenderPass->Unbind();
	}
#endif

	void VulkanGeometryPass::OnRenderDebug()
	{

	}

	void VulkanGeometryPass::OcclusionCullUpdate(const RenderQueue& renderQueue, uint64_t indirectCmdsOffset)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Data;

		// Occlusion Culling Part2:
		{
			auto vulkanComputePipeline = m_Data->LateCullPipeline.As<VulkanComputePipeline>();

			auto vulkanComputeDescriptor = m_Data->LateCullDescriptor[currentFrameIndex].As<VulkanMaterial>();
			vulkanComputeDescriptor->Bind(cmdBuf, m_Data->LateCullPipeline);


			m_Data->ComputeShaderPushConstant.DepthPyramidSize.z = indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand);

			m_Data->ComputeShaderPushConstant.CamFar = renderQueue.m_Camera->GetFarClip();
			m_Data->ComputeShaderPushConstant.CamNear = renderQueue.m_Camera->GetNearClip();
			m_Data->ComputeShaderPushConstant.ViewMatrix = renderQueue.m_Camera->GetViewMatrix();
			m_Data->ComputeShaderPushConstant.ProjectionMaxtrix = renderQueue.m_Camera->GetProjectionMatrix();
			m_Data->ComputeShaderPushConstant.ProjectionMaxtrix[1][1] *= -1;

			vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &m_Data->ComputeShaderPushConstant);

			uint32_t workGroupsX = std::floor((indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand)) / 64.0f) + 1;
			vulkanComputePipeline->Dispatch(cmdBuf, workGroupsX, 1, 1);

			vulkanIndirectCmdBuffer->SetMemoryBarrier(cmdBuf,
				VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
			);

		}
	}

	void VulkanGeometryPass::OnResize(uint32_t width, uint32_t height)
	{
		GeometryDataInit(width, height);

		Renderer::SubmitImageToOutputImageMap("Albedo", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->GeometryRenderPass->GetColorAttachment(2, currentFrameIndex);
		});
	}

	void VulkanGeometryPass::OnResizeLate(uint32_t width, uint32_t height)
	{
		OcclusionCullDataInit(width, height);
	}

	void VulkanGeometryPass::ShutDown()
	{
		delete m_Data;
	}
}