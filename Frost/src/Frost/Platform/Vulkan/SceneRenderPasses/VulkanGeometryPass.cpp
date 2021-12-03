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
	static uint64_t s_MaxCountSubmeshes = std::pow(2, 12); // 4096

	void VulkanGeometryPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
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

		m_Data->Descriptor.resize(framesInFlight);
		for (auto& material : m_Data->Descriptor)
		{
			material = Material::Create(m_Data->Shader, "GeometryPassIndirectBindless");

			auto whiteTexture = Renderer::GetWhiteLUT();
			for (uint32_t i = 0; i < 64; i++)
			{
				material->Set("u_AlbedoTexture", whiteTexture, i);
				material->Set("u_NormalTexture", whiteTexture, i);
				material->Set("u_MetalnessTexture", whiteTexture, i);
				material->Set("u_RoughnessTexture", whiteTexture, i);
			}

		}

		// Indirect drawing buffer
		m_Data->IndirectCmdBuffer.resize(framesInFlight);
		for (auto& indirectCmdBuffer : m_Data->IndirectCmdBuffer)
		{
			indirectCmdBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * s_MaxCountSubmeshes, { BufferUsage::Storage, BufferUsage::Indirect });
		}

		m_Data->InstanceData.resize(framesInFlight);
		for (auto& instanceData : m_Data->InstanceData)
		{
			instanceData.DeviceBuffer = BufferDevice::Create(sizeof(InstanceData) * s_MaxCountSubmeshes, { BufferUsage::Storage });
			instanceData.HostBuffer.Allocate(sizeof(InstanceData) * s_MaxCountSubmeshes);
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




		Vector<uint32_t> submeshOffsets;
		Vector<VkDrawIndexedIndirectCommand> indirectCmds;
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			auto submeshes = mesh->GetSubMeshes();

			// Set commands for the submeshes
			for (auto& submesh : submeshes)
			{
				glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;

				VkDrawIndexedIndirectCommand indirectCmdBuf{};
				indirectCmdBuf.firstIndex = submesh.BaseIndex;
				indirectCmdBuf.firstInstance = 0;
				indirectCmdBuf.indexCount = submesh.IndexCount;
				indirectCmdBuf.instanceCount = 1;
				indirectCmdBuf.vertexOffset = submesh.BaseVertex;

				indirectCmds.push_back(indirectCmdBuf);

#if 0
				auto meshMaterial = mesh->GetVulkanMaterial()[submesh.MaterialIndex].As<VulkanMaterial>();


				auto albedoTexture = meshMaterial->GetTexture2D("u_AlbedoTexture");
				m_Data->Descriptor[currentFrameIndex]->Set("u_AlbedoTexture", albedoTexture, i + submesh.MaterialIndex);

				auto normalTexture = meshMaterial->GetTexture2D("u_NormalTexture");
				m_Data->Descriptor[currentFrameIndex]->Set("u_NormalTexture", normalTexture, i + submesh.MaterialIndex);

				auto metalnessTexture = meshMaterial->GetTexture2D("u_MetalnessTexture");
				m_Data->Descriptor[currentFrameIndex]->Set("u_MetalnessTexture", metalnessTexture, i + submesh.MaterialIndex);

				auto roughnessTexture = meshMaterial->GetTexture2D("u_RoughnessTexture");
				m_Data->Descriptor[currentFrameIndex]->Set("u_RoughnessTexture", roughnessTexture, i + submesh.MaterialIndex);
#endif
			}


			if (submeshOffsets.size() == 0)
			{
				submeshOffsets.push_back(0);
			}
			else
			{
				auto previousMesh = renderQueue.m_Data[i - 1].Mesh;
				auto previousSubmeshes = previousMesh->GetSubMeshes();

				uint32_t previousSubmeshCount = previousSubmeshes.size();
				submeshOffsets.push_back(submeshOffsets[submeshOffsets.size() - 1] + previousSubmeshCount);
			}

		}

		// Sending the data into the gpu buffer
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].As<VulkanBufferDevice>();
		vulkanIndirectCmdBuffer->SetData(indirectCmds.size() * sizeof(VkDrawIndexedIndirectCommand), indirectCmds.data());


		m_Data->Descriptor[currentFrameIndex]->Bind(m_Data->Pipeline);

		uint32_t materialOffset = 0;
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{

			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;

			// Bind the vertex/index buffers
			mesh->GetVertexBuffer()->Bind();
			mesh->GetIndexBuffer()->Bind();

			for (uint32_t j = 0; j < mesh->GetSubMeshes().size(); j++)
			{
				auto submesh = mesh->GetSubMeshes()[j];
				glm::mat4 modelMatrix = renderQueue.m_Data[i].Transform * submesh.Transform;

				// Get the submesh material and bind it
				//Ref<VulkanMaterial> material = mesh->GetVulkanMaterial()[submesh.MaterialIndex].As<VulkanMaterial>();
				//material->Bind(m_Data->Pipeline);

				
				// Set the transform matrix and model matrix of the submesh into a constant buffer
				PushConstant pushConstant;
				pushConstant.TransformMatrix = viewProjectionMatrix * modelMatrix;
				pushConstant.ModelMatrix = modelMatrix;
				pushConstant.MaterialIndex = submeshOffsets[i];
				vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

				uint32_t submeshCount = mesh->GetSubMeshes().size();
				uint32_t offset = submeshOffsets[i] * sizeof(VkDrawIndexedIndirectCommand);
				vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));


#if 0
				// TODO: Do this if multi-draw isn't detected
				uint32_t offset = (i + j) * sizeof(VkDrawIndexedIndirectCommand);
				vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, 1, sizeof(VkDrawIndexedIndirectCommand));
#endif


				break;
			}
		}


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