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
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferDevice.h"

namespace Frost
{

	VulkanGeometryPass::VulkanGeometryPass()
		: m_Name("GeometryPass")
	{
	}

	VulkanGeometryPass::~VulkanGeometryPass()
	{
		delete m_Data;
	}

	static RenderPassSpecification s_RenderPassSpec;

	void VulkanGeometryPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().GeometryPass_Mesh_Count;

		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->Shader = Renderer::GetShaderLibrary()->Get("GeometryPassIndirectBindless");

		s_RenderPassSpec =
		{
			1600, 900, 3,
			{
				// Position Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Normals Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Albedo Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Composite Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// ECS Attachment
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Depth Attachment
				{
					FramebufferTextureFormat::DepthStencil, ImageUsage::DepthStencil,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				}
			}
		};
		m_Data->RenderPass = RenderPass::Create(s_RenderPassSpec);

		BufferLayout bufferLayout = {
			{ "a_Position",      ShaderDataType::Float3 },
			{ "a_TexCoord",	     ShaderDataType::Float2 },
			{ "a_Normal",	     ShaderDataType::Float3 },
			{ "a_Tangent",	     ShaderDataType::Float3 },
			{ "a_Bitangent",     ShaderDataType::Float3 },
			{ "a_MaterialIndex", ShaderDataType::Float }
		};
		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->Shader;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.UseStencil = false;
		pipelineCreateInfo.RenderPass = m_Data->RenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data->Pipeline = Pipeline::Create(pipelineCreateInfo);




		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		auto whiteTexture = Renderer::GetWhiteLUT();

		m_Data->Descriptor.resize(framesInFlight);
		for (auto& material : m_Data->Descriptor)
		{
			material = Material::Create(m_Data->Shader, "GeometryPassIndirectBindless");
		}


		// Indirect drawing buffer
		m_Data->IndirectCmdBuffer.resize(framesInFlight);
		for (auto& indirectCmdBuffer : m_Data->IndirectCmdBuffer)
		{
			// Allocating a heap block
			indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes,
																 { BufferUsage::Storage, BufferUsage::Indirect });

			indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes);
		}

		// Instance data storage buffer
		m_Data->InstanceSpecs.resize(framesInFlight);
		for (uint32_t i = 0; i < m_Data->InstanceSpecs.size(); i++)
		{
			auto& instanceSpec = m_Data->InstanceSpecs[i];

			// Allocating a heap block
			instanceSpec.DeviceBuffer = BufferDevice::Create(sizeof(InstanceData) * maxCountMeshes, { BufferUsage::Storage });
			instanceSpec.HostBuffer.Allocate(sizeof(InstanceData) * maxCountMeshes);

			// Setting the storage buffer into the descriptor
			m_Data->Descriptor[i]->Set("u_MaterialUniform", instanceSpec.DeviceBuffer);
		}
	}

	void VulkanGeometryPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;

		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->RenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->Pipeline.As<VulkanPipeline>();

		// Bind the pipeline and renderpass
		m_Data->RenderPass->Bind();
		m_Data->Pipeline->Bind();
		
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


		// Get the needed matricies from the camera
		glm::mat4 projectioMatrix = renderQueue.CameraProjectionMatrix;
		projectioMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		glm::mat4 viewProjectionMatrix = projectioMatrix * renderQueue.CameraViewMatrix;



		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
		*/
		Vector<uint32_t> meshOffsets;

		// `Indirect draw commands` offset
		uint32_t indirectCmdsOffset = 0;

		// `Instance data` offset.
		//  Allocating here the `InstanceData`, because if we allocate it for every submesh in the scene, it will be way more expensive
		uint32_t instanceDataOffset = 0;
		InstanceData instData{};

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetSubMeshes();

			// Set commands for the submeshes
			for (uint32_t k = 0; k < submeshes.size(); k++)
			{
				const Submesh& submesh = submeshes[k];

				// Submit the submesh into the cpu buffer
				VkDrawIndexedIndirectCommand indirectCmdBuf{};
				indirectCmdBuf.firstIndex = submesh.BaseIndex;
				indirectCmdBuf.firstInstance = 0;
				indirectCmdBuf.indexCount = submesh.IndexCount;
				indirectCmdBuf.instanceCount = 1;
				indirectCmdBuf.vertexOffset = submesh.BaseVertex;
				m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);


				// Setting up the material data into a storage buffer
				DataStorage& materialData = mesh->GetMaterialData(k);

				// Textures
				instData.AlbedoTextureID =     materialData.Get<uint32_t>("AlbedoTexture");
				instData.RoughessTextureID =   materialData.Get<uint32_t>("RoughnessTexture");
				instData.MetalnessTextureID =  materialData.Get<uint32_t>("MetalnessTexture");
				instData.NormalTextureID =     materialData.Get<uint32_t>("NormalTexture");

				instData.UseNormalMap =		   materialData.Get<uint32_t>("UseNormalMap");

				// PBR values
				instData.AlbedoColor =         glm::vec4(materialData.Get<glm::vec3>("AlbedoColor"), 1.0f);
				instData.Roughness =           materialData.Get<float>("RoughnessFactor");
				instData.Metalness =           materialData.Get<float>("MetalnessFactor");
				instData.Emission =            materialData.Get<float>("EmissionFactor");

				// Matricies
				instData.ModelMatrix =       renderQueue.m_Data[i].Transform * materialData.Get<glm::mat4>("ModelMatrix");
				instData.WorldSpaceMatrix =  viewProjectionMatrix * instData.ModelMatrix;

				// The data collected from the mesh, should be written into a cpu buffer, which will later be copied into a storage buffer
				m_Data->InstanceSpecs[currentFrameIndex].HostBuffer.Write((void*)&instData, sizeof(InstanceData), instanceDataOffset);

				// Adding up the offset
				instanceDataOffset += sizeof(InstanceData);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
			}


			// If we are submitting the first mesh, we don't need any offset
			if (meshOffsets.size() == 0)
			{
				meshOffsets.push_back(0);
			}
			else
			{
				// If it is not the first mesh, then we set the offset as the last mesh's submesh count
				auto previousMesh = renderQueue.m_Data[i - 1].Mesh;
				auto previousSubmeshes = previousMesh->GetSubMeshes();

				uint32_t previousSubmeshCount = previousSubmeshes.size();
				meshOffsets.push_back(meshOffsets[meshOffsets.size() - 1] + previousSubmeshCount);
			}
		}

		// Sending the data into the gpu buffer
		// Indirect draw commands
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Data;
		vulkanIndirectCmdBuffer->SetData(indirectCmdsOffset, indirectCmdsPointer);

		// Instance data
		auto vulkanInstanceDataBuffer = m_Data->InstanceSpecs[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* instanceDataPointer = m_Data->InstanceSpecs[currentFrameIndex].HostBuffer.Data;
		vulkanInstanceDataBuffer->SetData(instanceDataOffset, instanceDataPointer);




		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->Pipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->Descriptor[currentFrameIndex].As<VulkanMaterial>();
		vulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
		Vector<VkDescriptorSet> descriptorSets = vulkanDescriptor->GetVulkanDescriptorSets();
		descriptorSets[1] = VulkanBindlessAllocator::GetVulkanDescriptorSet(currentFrameIndex);
		
		vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), 0, nullptr);
		//m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);


		// Sending the indirect draw commands to the command buffer
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{

			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;

			// Bind the vertex/index buffers
			mesh->GetVertexBuffer()->Bind();
			mesh->GetIndexBuffer()->Bind();

			// Set the transform matrix and model matrix of the submesh into a constant buffer
			PushConstant pushConstant;
			pushConstant.MaterialIndex = meshOffsets[i];
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

			uint32_t submeshCount = mesh->GetSubMeshes().size();
			uint32_t offset = meshOffsets[i] * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));


#if 0
			// TODO: Do this if multi-draw isn't detected
			uint32_t offset = (i + j) * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, 1, sizeof(VkDrawIndexedIndirectCommand));
#endif
		}


		// DrawIndexed
#if 0
		{

			for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
			{
				// Get the mesh
				auto mesh = renderQueue.m_Data[i].Mesh;

				// Bind the vertex/index buffers
				mesh->GetVertexBuffer()->Bind();
				mesh->GetIndexBuffer()->Bind();

				// Draw every submesh
				for (auto& submesh : mesh->GetSubMeshes())
				{
					// Check if the submesh is seen by the camera (frustum culling)
					glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;
					if (!submesh.BoundingBox.IsOnFrustum(renderQueue.Camera.GetFrustum(), modelMatrix)) continue;

					// Get the submesh material and bind it
					Ref<VulkanMaterial> material = mesh->GetVulkanMaterial()[submesh.MaterialIndex].As<VulkanMaterial>();
					material->Bind(m_Data->Pipeline);

					// Set the transform matrix and model matrix of the submesh into a constant buffer
					PushConstant pushConstant;
					pushConstant.TransformMatrix = viewProjectionMatrix * modelMatrix;
					pushConstant.ModelMatrix = modelMatrix;
					vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

					// Draw the submesh
					vkCmdDrawIndexed(cmdBuf, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
				}
			}
		}
#endif
		// End the renderpass
		m_Data->RenderPass->Unbind();
	}

	void VulkanGeometryPass::OnResize(uint32_t width, uint32_t height)
	{
		s_RenderPassSpec.FramebufferSpecification.Width = width;
		s_RenderPassSpec.FramebufferSpecification.Height = height;
		m_Data->RenderPass = RenderPass::Create(s_RenderPassSpec);
	}

	void VulkanGeometryPass::ShutDown()
	{
		delete m_Data;
	}
}