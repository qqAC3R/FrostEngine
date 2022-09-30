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

		m_Data->GeometryShader = Renderer::GetShaderLibrary()->Get("GeometryPassIndirectBindless");
		m_Data->LateCullShader = Renderer::GetShaderLibrary()->Get("OcclusionCulling_V2");


		GeometryDataInit(1600, 900);

		Renderer::SubmitImageToOutputImageMap("Albedo", [this]() -> Ref<Image2D>
		{
			return this->m_Data->GeometryRenderPass->GetColorAttachment(2, 0);
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
		/*
		BufferLayout bufferLayout = {
			{ "a_Position",      ShaderDataType::Float3 },
			{ "a_TexCoord",	     ShaderDataType::Float2 },
			{ "a_Normal",	     ShaderDataType::Float3 },
			{ "a_Tangent",	     ShaderDataType::Float3 },
			{ "a_Bitangent",     ShaderDataType::Float3 },
			{ "a_MaterialIndex", ShaderDataType::Float }
		};
		*/
		BufferLayout bufferLayout = {
			{ "a_ModelSpaceMatrix",  ShaderDataType::Mat4 },
			{ "a_WorldSpaceMatrix",  ShaderDataType::Mat4 },
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
			if(!material) material = Material::Create(m_Data->GeometryShader, "GeometryPassIndirectBindless");
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


				// Mesh specs for the compute shader (occlusion culling) TODO: Only 1??
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

	static Vector<IndirectMeshData> s_Geometry_MeshIndirectData;

	void VulkanGeometryPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;


		VulkanRenderer::BeginTimeStampPass("Geometry Pass");
		GeometryPrepareIndirectData(renderQueue);
		GeometryUpdate(renderQueue, s_Geometry_MeshIndirectData);
		VulkanRenderer::EndTimeStampPass("Geometry Pass");
	}


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
		//  Allocating here the `InstanceData`, because if we allocate it for every submesh in the scene, it will be way more expensive
		//MaterialData instData{};
		uint32_t materialDataOffset = 0;

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetSubMeshes();

			// Instanced data for submeshes
			//SubmeshInstanced submeshInstanced{};
			//uint32_t vboInstancedDataOffset = 0;

			// Count how many meshes were submitted (for calculating offsets)
			uint32_t submittedSubmeshes = 0;

			mesh->UpdateInstancedVertexBuffer(renderQueue.m_Data[i].Transform, viewProjectionMatrix, currentFrameIndex);

			// Set commands for the submeshes
			for (uint32_t k = 0; k < submeshes.size(); k++)
			{
				const Submesh& submesh = submeshes[k];

				glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;
				//glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform;


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

				// Submit instanced data into a cpu buffer (which will be later sent to the gpu's instanced vbo)
				//submeshInstanced.ModelSpaceMatrix = modelMatrix;
				//submeshInstanced.WorldSpaceMatrix = viewProjectionMatrix * modelMatrix;
				//mesh->GetVertexBufferInstanced_CPU(currentFrameIndex).Write((void*)&submeshInstanced, sizeof(SubmeshInstanced), vboInstancedDataOffset);


				// Adding up the offset
				meshDataOffset += sizeof(MeshData_OC);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
				submittedSubmeshes += 1;
				//vboInstancedDataOffset += sizeof(SubmeshInstanced);
			}

			// Submit instanced data from a cpu buffer to gpu vertex buffer
			//auto vulkanVBOInstanced = mesh->GetVertexBufferInstanced(currentFrameIndex).As<VulkanBufferDevice>();
			//vulkanVBOInstanced->SetData(vboInstancedDataOffset, mesh->GetVertexBufferInstanced_CPU(currentFrameIndex).Data);


			for (uint32_t k = 0; k < mesh->GetMaterialCount(); k++)
			{
				// Setting up the material data into a storage buffer
				DataStorage& materialData = mesh->GetMaterialData(k);

#if 0
				// Textures
				instData.AlbedoTextureID = materialData.Get<uint32_t>("AlbedoTexture");
				instData.RoughessTextureID = materialData.Get<uint32_t>("RoughnessTexture");
				instData.MetalnessTextureID = materialData.Get<uint32_t>("MetalnessTexture");
				instData.NormalTextureID = materialData.Get<uint32_t>("NormalTexture");
				
				instData.UseNormalMap = materialData.Get<uint32_t>("UseNormalMap");
				
				// PBR values
				instData.AlbedoColor = glm::vec4(materialData.Get<glm::vec3>("AlbedoColor"), 1.0f);
				instData.Roughness = materialData.Get<float>("RoughnessFactor");
				instData.Metalness = materialData.Get<float>("MetalnessFactor");
				instData.Emission = materialData.Get<float>("EmissionFactor");

				// The data collected from the mesh, should be written into a cpu buffer, which will later be copied into a storage buffer
				//m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Write((void*)&instData, sizeof(MaterialData), materialDataOffset);
#endif

				m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Write((void*)materialData, sizeof(MaterialData), materialDataOffset);

				materialDataOffset += sizeof(MaterialData);
			}

			// If we are submitting the first mesh, we don't need any offset
			if (s_Geometry_MeshIndirectData.size() == 0)
			{
				s_Geometry_MeshIndirectData.emplace_back(IndirectMeshData(0, submeshes.size(), i, mesh->GetMaterialCount(), 0));
			}
			else
			{
				uint32_t previousMeshOffset = s_Geometry_MeshIndirectData[i - 1].SubmeshOffset;
				uint32_t previousMeshCount = s_Geometry_MeshIndirectData[i - 1].SubmeshCount;
				uint32_t currentMeshOffset = previousMeshOffset + previousMeshCount;

				uint32_t previousMaterialOffset = s_Geometry_MeshIndirectData[i - 1].MaterialOffset;
				uint32_t previousMaterialCount = s_Geometry_MeshIndirectData[i - 1].MaterialCount;
				uint32_t currentMaterialOffset = previousMaterialOffset + previousMaterialCount;

				s_Geometry_MeshIndirectData.emplace_back(IndirectMeshData(currentMeshOffset, submeshes.size(), i, mesh->GetMaterialCount(), currentMaterialOffset));

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

		// Mesh data (occlusion culling)
		auto vulkanMeshDataBuffer = m_Data->MeshSpecs.DeviceBuffer.As<VulkanBufferDevice>();
		void* meshDataPointer = m_Data->MeshSpecs.HostBuffer.Data;
		vulkanMeshDataBuffer->SetData(meshDataOffset, meshDataPointer);


		//OcclusionCullUpdate(renderQueue, indirectCmdsOffset);

	}

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
			mesh->GetIndexBuffer()->Bind();

			// Bind the vertex buffer (only the instanced one, since for the "per vertex" one, we are using BDAs
			auto vulkanVertexBufferInstanced = mesh->GetVertexBufferInstanced(currentFrameIndex).As<VulkanBufferDevice>();
			VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
			VkDeviceSize deviceSize[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);


			// Set the transform matrix and model matrix of the submesh into a constant buffer
			PushConstant pushConstant;
			pushConstant.MaterialIndex = indirectMeshData[i].MaterialOffset;
			pushConstant.VertexBufferBDA = mesh->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			pushConstant.ViewMatrix = renderQueue.CameraViewMatrix;
			pushConstant.IsAnimated = static_cast<uint32_t>(mesh->IsAnimated());

			if(mesh->IsAnimated())
				pushConstant.BoneInformationBDA = mesh->GetBoneUniformBuffer(currentFrameIndex).As<VulkanUniformBuffer>()->GetVulkanBufferAddress();
			else
				pushConstant.BoneInformationBDA = 0;

			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

			uint32_t submeshCount = meshData.SubmeshCount;
			uint32_t offset = indirectMeshData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));
		}
		// End the renderpass
		m_Data->GeometryRenderPass->Unbind();
	}

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