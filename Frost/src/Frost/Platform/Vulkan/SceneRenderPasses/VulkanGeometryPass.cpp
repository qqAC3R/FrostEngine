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

#include "Frost/Platform/Vulkan/VulkanPipelineCompute.h"

#include <imgui.h>
#include "Frost/ImGui/ImGuiLayer.h"

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
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->GeometryShader = Renderer::GetShaderLibrary()->Get("GeometryPassIndirectBindless");
		//m_Data->LateCullShader = Renderer::GetShaderLibrary()->Get("OcclusionCulling");
		m_Data->LateCullShader = Renderer::GetShaderLibrary()->Get("OcclusionCulling_V2");
		m_Data->HZBShader = Renderer::GetShaderLibrary()->Get("HiZBufferBuilder");


		Geometry_DataInit();
		HZB_DataInit();
		LateCull_DataInit();
	}


	/// Geometry pass initialization
	void VulkanGeometryPass::Geometry_DataInit()
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().GeometryPass_Mesh_Count;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


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
#if 0
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
#endif

				// Depth Attachment
				{
					FramebufferTextureFormat::Depth, ImageUsage::DepthStencil,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				}
			}
		};
		m_Data->RenderPass = RenderPass::Create(s_RenderPassSpec);


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
		pipelineCreateInfo.RenderPass = m_Data->RenderPass;
		pipelineCreateInfo.VertexBufferLayout = bufferLayout;
		m_Data->Pipeline = Pipeline::Create(pipelineCreateInfo);


		/// Geometry pass descriptors (from PBR Shader)
		m_Data->Descriptor.resize(framesInFlight);
		for (auto& material : m_Data->Descriptor)
		{
			material = Material::Create(m_Data->GeometryShader, "GeometryPassIndirectBindless");
		}


		/// Indirect drawing buffer
		m_Data->IndirectCmdBuffer.resize(framesInFlight);
		for (auto& indirectCmdBuffer : m_Data->IndirectCmdBuffer)
		{
			// Allocating a heap block
			indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * maxCountMeshes,
				{ BufferUsage::Storage, BufferUsage::Indirect });

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
			m_Data->Descriptor[i]->Set("u_MaterialUniform", instanceSpec.DeviceBuffer);
		}

	}

	void VulkanGeometryPass::HZB_DataInit()
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().GeometryPass_Mesh_Count;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		//////////////////////////////////////////////
		// DEPTH PYRAMID CONSTRUCTION ////////////////
		//////////////////////////////////////////////
		{
			// Calculating the previous frame index
			{
				m_Data->HZB_Dimensions = glm::vec2(1600, 900);

				m_Data->DepthPyramid.resize(framesInFlight);
				for (uint32_t i = 0; i < framesInFlight; i++)
				{

					ImageSpecification imageSpec{};
					imageSpec.Width = 1600;
					imageSpec.Height = 900;
					imageSpec.Sampler.ReductionMode_Optional = ReductionMode::Min;
					imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
					imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

					imageSpec.Format = ImageFormat::R32;
					imageSpec.Usage = ImageUsage::Storage;
					imageSpec.UseMipChain = true;

					m_Data->DepthPyramid[i] = Image2D::Create(imageSpec);
				}
			}

			// Pipeline creation
			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->HZBShader;
			m_Data->HZBPipeline = ComputePipeline::Create(computePipelineCreateInfo);


			// Descriptor creation
			uint32_t mipLevels = (uint32_t)std::floor(std::log2(std::max(1600, 900))) + 1;
			m_Data->HZB_MipLevels = mipLevels;
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			m_Data->HZBDescriptor.resize(framesInFlight);
			for (uint32_t j = 0; j < framesInFlight; j++)
			{
				m_Data->HZBDescriptor[j].resize(mipLevels);
				for (uint32_t i = 0; i < mipLevels; i++)
				{
					auto& material = m_Data->HZBDescriptor[j][i];
					material = Material::Create(m_Data->HZBShader, "Hi-Z_Buffer_Builder");


					auto& vulkanDescriptor = material.As<VulkanMaterial>();
					auto& vulkanDepthPyramid = m_Data->DepthPyramid[j].As<VulkanImage2D>();

					VkDescriptorSet descriptorSet = vulkanDescriptor->GetVulkanDescriptorSet(0);

					// If we use the first descriptor set, we only will set the `o_Depth` texture (because the input will be last's frame depth buffer)
					if (i == 0)
					{
						VkDescriptorImageInfo imageDescriptorInfo{};
						imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(0);
						imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
						imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

						VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						writeDescriptorSet.dstBinding = 1;
						writeDescriptorSet.dstArrayElement = 0;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

						continue;
					}


					// Setting up `i_Depth`
					{
						VkDescriptorImageInfo imageDescriptorInfo{};
						imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(i - 1);
						imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
						imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

						VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						writeDescriptorSet.dstBinding = 0;
						writeDescriptorSet.dstArrayElement = 0;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
					}

					// Setting up `o_Depth`
					{
						VkDescriptorImageInfo imageDescriptorInfo{};
						imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(i);
						imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
						imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

						VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						writeDescriptorSet.dstBinding = 1;
						writeDescriptorSet.dstArrayElement = 0;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
					}
				}
			}
		}
	}

	void VulkanGeometryPass::LateCull_DataInit()
	{
		uint64_t maxCountMeshes = Renderer::GetRendererConfig().GeometryPass_Mesh_Count;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		//////////////////////////////////////////////
		// LATE CULLING //////////////////////////////
		//////////////////////////////////////////////
		{

			ComputePipeline::CreateInfo computePipelineCreateInfo{};
			computePipelineCreateInfo.Shader = m_Data->LateCullShader;
			m_Data->LateCullPipeline = ComputePipeline::Create(computePipelineCreateInfo);

			m_Data->DebugDeviceBuffer = BufferDevice::Create(sizeof(glm::mat4) * 1024, { BufferUsage::Storage });


			// Mesh specs for the compute shader (occlusion culling) TODO: Only 1??
			m_Data->MeshSpecs.DeviceBuffer = BufferDevice::Create(sizeof(MeshData_OC) * maxCountMeshes, { BufferUsage::Storage });
			m_Data->MeshSpecs.HostBuffer.Allocate(sizeof(MeshData_OC) * maxCountMeshes);

			m_Data->LateCullDescriptor.resize(framesInFlight);
			for (uint32_t i = 0; i < m_Data->LateCullDescriptor.size(); i++)
			{
				auto& computeDescriptor = m_Data->LateCullDescriptor[i];

				computeDescriptor = Material::Create(m_Data->LateCullShader, "OcclusionCulling");
				computeDescriptor->Set("DepthPyramid", m_Data->DepthPyramid[i]);
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

		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->RenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->Pipeline.As<VulkanPipeline>();

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

		// Data for occlusion culling shader
		MeshData_OC meshData{};
		uint32_t meshDataOffset = 0;

		// `Instance data` offset.
		//  Allocating here the `InstanceData`, because if we allocate it for every submesh in the scene, it will be way more expensive
		MaterialData instData{};
		uint32_t materialDataOffset = 0;

		// Get all the indirect draw commands
		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;
			const Vector<Submesh>& submeshes = mesh->GetSubMeshes();

			// Instanced data for submeshes
			SubmeshInstanced submeshInstanced{};
			uint32_t vboInstancedDataOffset = 0;

			// Count how many meshes were submitted (for calculating offsets)
			uint32_t submittedSubmeshes = 0;

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
				//instData.ModelMatrix =		 modelMatrix;
				//instData.WorldSpaceMatrix =  viewProjectionMatrix * instData.ModelMatrix;

				// The data collected from the mesh, should be written into a cpu buffer, which will later be copied into a storage buffer
				m_Data->MaterialSpecs[currentFrameIndex].HostBuffer.Write((void*)&instData, sizeof(MaterialData), materialDataOffset);



				// Setting up `Mesh data` for the occlusion culling compute shader
				meshData.Transform = modelMatrix;
				meshData.AABB_Min = glm::vec4(submesh.BoundingBox.Min, 1.0f);
				meshData.AABB_Max = glm::vec4(submesh.BoundingBox.Max, 1.0f);
				m_Data->MeshSpecs.HostBuffer.Write((void*)&meshData, sizeof(MeshData_OC), meshDataOffset);

				// Submit instanced data into a cpu buffer (which will be later sent to the gpu's instanced vbo)
				submeshInstanced.ModelSpaceMatrix = modelMatrix;
				submeshInstanced.WorldSpaceMatrix = viewProjectionMatrix * modelMatrix;
				mesh->GetVertexBufferInstanced_CPU().Write((void*)&submeshInstanced, sizeof(SubmeshInstanced), vboInstancedDataOffset);

				// Adding up the offset
				meshDataOffset     += sizeof(MeshData_OC);
				materialDataOffset += sizeof(MaterialData);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
				submittedSubmeshes += 1;
				vboInstancedDataOffset += sizeof(SubmeshInstanced);
			}

			// Submit instanced data from a cpu buffer to gpu vertex buffer
			auto vulkanVBOInstanced = mesh->GetVertexBufferInstanced().As<VulkanBufferDevice>();
			vulkanVBOInstanced->SetData(vboInstancedDataOffset, mesh->GetVertexBufferInstanced_CPU().Data);

#if 0
			if (!meshIndirectData.size())
			{
				if (submittedSubmeshes != 0)
				{
					meshIndirectData.emplace_back(IndirectMeshData(0, submittedSubmeshes, i));
				}
			}
			else
			{

				if (i != renderQueue.GetQueueSize() - 1)
				{
					if (submittedSubmeshes != 0)
					{
						uint32_t previousMeshOffset = meshIndirectData[i - 1].SubmeshOffset;
						uint32_t previousMeshCount = meshIndirectData[i - 1].SubmeshCount;

						uint32_t currentOffset = previousMeshOffset + previousMeshCount;

						meshIndirectData.emplace_back(IndirectMeshData(currentOffset, submittedSubmeshes, i));
					}
				}
			}
#else
			// If we are submitting the first mesh, we don't need any offset
			if (meshIndirectData.size() == 0)
			{
				meshIndirectData.emplace_back(IndirectMeshData(0, submeshes.size(), i));
			}
			else
			{
				uint32_t previousMeshOffset = meshIndirectData[i - 1].SubmeshOffset;
				uint32_t previousMeshCount = meshIndirectData[i - 1].SubmeshCount;

				uint32_t currentOffset = previousMeshOffset + previousMeshCount;

				meshIndirectData.emplace_back(IndirectMeshData(currentOffset, submeshes.size(), i));

				// If it is not the first mesh, then we set the offset as the last mesh's submesh count
				//auto previousMesh = renderQueue.m_Data[i - 1].Mesh;
				//auto previousSubmeshes = previousMesh->GetSubMeshes();
				//
				//uint32_t previousSubmeshCount = previousSubmeshes.size();

				//meshIndirectData.emplace_back(meshOffsets[meshOffsets.size() - 1] + previousSubmeshCount);

			}
#endif


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

		// Mesh data
		auto vulkanMeshDataBuffer = m_Data->MeshSpecs.DeviceBuffer.As<VulkanBufferDevice>();
		void* meshDataPointer = m_Data->MeshSpecs.HostBuffer.Data;
		vulkanMeshDataBuffer->SetData(meshDataOffset, meshDataPointer);


		OcclusionCullUpdate(renderQueue, indirectCmdsOffset);






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


		// TODO: This is so bad, pls fix this
		VkPipelineLayout pipelineLayout = m_Data->Pipeline.As<VulkanPipeline>()->GetVulkanPipelineLayout();

		auto vulkanDescriptor = m_Data->Descriptor[currentFrameIndex].As<VulkanMaterial>();
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
			auto vulkanVertexBufferInstanced = mesh->GetVertexBufferInstanced().As<VulkanBufferDevice>();
			VkBuffer vertexBufferInstanced = vulkanVertexBufferInstanced->GetVulkanBuffer();
			VkDeviceSize deviceSize[1] = { 0 };
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &vertexBufferInstanced, deviceSize);


			// Set the transform matrix and model matrix of the submesh into a constant buffer
			PushConstant pushConstant;
			pushConstant.MaterialIndex = meshIndirectData[i].SubmeshOffset;
			pushConstant.VertexBufferBDA = mesh->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

			uint32_t submeshCount = meshData.SubmeshCount;
			uint32_t offset = meshIndirectData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));
		}

#if 0
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
			pushConstant.MaterialIndex = meshIndirectData[i].SubmeshOffset;
			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&pushConstant);

			uint32_t submeshCount = mesh->GetSubMeshes().size();
			uint32_t offset = meshIndirectData[i].SubmeshOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, submeshCount, sizeof(VkDrawIndexedIndirectCommand));


#if 0
			// TODO: Do this if multi-draw isn't detected
			uint32_t offset = (i + j) * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, 1, sizeof(VkDrawIndexedIndirectCommand));
#endif
		}
#endif


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

	void VulkanGeometryPass::OcclusionCullUpdate(const RenderQueue& renderQueue, uint64_t indirectCmdsOffset)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* indirectCmdsPointer = m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Data;

		// Occlusion culling
		{
			int32_t previousFrameIndex = (int32_t)currentFrameIndex - 1;
			if (previousFrameIndex < 0)
				previousFrameIndex = Renderer::GetRendererConfig().FramesInFlight - 1;

			auto srcDepthImage = m_Data->RenderPass->GetDepthAttachment(previousFrameIndex);
			auto vulkanSrcDepthImage = srcDepthImage.As<VulkanImage2D>();

			VkImageLayout srcDepthImageLayout = vulkanSrcDepthImage->GetVulkanImageLayout();
			vulkanSrcDepthImage->TransitionLayout(cmdBuf, srcDepthImageLayout, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);


#if 0
			// Debug
			ImGui::Begin("HZB");
			ImTextureID depthPyramidID = ImGuiLayer::GetTextureIDFromVulkanTexture_MipLevel(m_Data->DepthPyramid[currentFrameIndex], (uint32_t)s_DebugMipLevel);
			ImGui::Image(depthPyramidID, { 400, 300 });
			ImGui::SliderInt("MipLevel", &s_DebugMipLevel, 0, 10);
			ImGui::End();
#endif

			// Occlusion Culling Part1:
			glm::vec2 currentHZB_Dimensions = m_Data->HZB_Dimensions;
			for (uint32_t mipLevel = 0; mipLevel < m_Data->HZB_MipLevels; mipLevel++)
			{

				auto vulkanHZB_Pipeline = m_Data->HZBPipeline.As<VulkanComputePipeline>();
				auto vulkanHZB_Descriptor = m_Data->HZBDescriptor[currentFrameIndex][mipLevel].As<VulkanMaterial>();
				auto vulkanDepthPyramid = m_Data->DepthPyramid[currentFrameIndex].As<VulkanImage2D>();

				if (mipLevel == 0)
				{
					vulkanHZB_Descriptor->Set("i_Depth", srcDepthImage);
				}
				else
				{
					currentHZB_Dimensions.x /= 2;
					currentHZB_Dimensions.y /= 2;
				}

				vulkanHZB_Descriptor->Bind(cmdBuf, m_Data->HZBPipeline);

				vulkanHZB_Pipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &currentHZB_Dimensions);




				vulkanHZB_Pipeline->Dispatch(cmdBuf, std::floor(currentHZB_Dimensions.x / 32.0f) + 1, std::floor(currentHZB_Dimensions.y / 32.0f) + 1, 1.0f);



				VkImageSubresourceRange imageSubrange{};
				imageSubrange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				imageSubrange.baseArrayLayer = 0;
				imageSubrange.baseMipLevel = mipLevel;
				imageSubrange.layerCount = 1;
				imageSubrange.levelCount = 1;

				Utils::InsertImageMemoryBarrier(cmdBuf, vulkanDepthPyramid->GetVulkanImage(),
					VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
					vulkanDepthPyramid->GetVulkanImageLayout(), vulkanDepthPyramid->GetVulkanImageLayout(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					imageSubrange
				);
			}

			// Occlusion Culling Part2:
			{
				auto vulkanComputePipeline = m_Data->LateCullPipeline.As<VulkanComputePipeline>();

				auto vulkanComputeDescriptor = m_Data->LateCullDescriptor[currentFrameIndex].As<VulkanMaterial>();
				vulkanComputeDescriptor->Bind(cmdBuf, m_Data->LateCullPipeline);


				m_Data->ComputeShaderPushConstant.DepthPyramidSize.z = indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand);

				m_Data->ComputeShaderPushConstant.CamFar = renderQueue.m_Camera.GetFarClip();
				m_Data->ComputeShaderPushConstant.CamNear = renderQueue.m_Camera.GetNearClip();
				m_Data->ComputeShaderPushConstant.ViewMatrix = renderQueue.m_Camera.GetViewMatrix();
				m_Data->ComputeShaderPushConstant.ProjectionMaxtrix = renderQueue.m_Camera.GetProjectionMatrix();
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
	}

	void VulkanGeometryPass::OnResize(uint32_t width, uint32_t height)
	{
		s_RenderPassSpec.FramebufferSpecification.Width = width;
		s_RenderPassSpec.FramebufferSpecification.Height = height;
		m_Data->RenderPass = RenderPass::Create(s_RenderPassSpec);



		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

		{

			// Depth pyramid creation
			{

				m_Data->HZB_Dimensions = glm::vec2(width, height);
				m_Data->HZB_MipLevels = (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;

				m_Data->ComputeShaderPushConstant.DepthPyramidSize.x = width;
				m_Data->ComputeShaderPushConstant.DepthPyramidSize.y = height;

				for (uint32_t i = 0; i < framesInFlight; i++)
				{
					ImageSpecification imageSpec{};
					imageSpec.Width = width;
					imageSpec.Height = height;
					imageSpec.Sampler.ReductionMode_Optional = ReductionMode::Min;
					imageSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
					imageSpec.Sampler.SamplerWrap = ImageWrap::ClampToEdge;

					imageSpec.Format = ImageFormat::R32;
					imageSpec.Usage = ImageUsage::Storage;
					imageSpec.UseMipChain = true;

					m_Data->DepthPyramid[i] = Image2D::Create(imageSpec);
				}
			}


			for (uint32_t i = 0; i < m_Data->LateCullDescriptor.size(); i++)
			{
				auto& material = m_Data->LateCullDescriptor[i];
				material->Set("DepthPyramid", m_Data->DepthPyramid[i]);
			}


			
			// Setting up the HZB builder descriptor
			for (uint32_t j = 0; j < framesInFlight; j++)
			{
				for (uint32_t i = 0; i < m_Data->HZB_MipLevels; i++)
				{
					auto& material = m_Data->HZBDescriptor[j][i];

					auto& vulkanDescriptor = material.As<VulkanMaterial>();
					auto& vulkanDepthPyramid = m_Data->DepthPyramid[j].As<VulkanImage2D>();

					VkDescriptorSet descriptorSet = vulkanDescriptor->GetVulkanDescriptorSet(0);

					// If we use the first descriptor set, we only will set the `o_Depth` texture (because the input will be last's frame depth buffer)
					if (i == 0)
					{
						VkDescriptorImageInfo imageDescriptorInfo{};
						imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(0);
						imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
						imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

						VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						writeDescriptorSet.dstBinding = 1;
						writeDescriptorSet.dstArrayElement = 0;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

						continue;
					}


					// Setting up `i_Depth`
					{
						VkDescriptorImageInfo imageDescriptorInfo{};
						imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(i - 1);
						imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
						imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

						VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						writeDescriptorSet.dstBinding = 0;
						writeDescriptorSet.dstArrayElement = 0;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
					}

					// Setting up `o_Depth`
					{
						VkDescriptorImageInfo imageDescriptorInfo{};
						imageDescriptorInfo.imageView = vulkanDepthPyramid->GetVulkanImageViewMip(i);
						imageDescriptorInfo.imageLayout = vulkanDepthPyramid->GetVulkanImageLayout();
						imageDescriptorInfo.sampler = vulkanDepthPyramid->GetVulkanSampler();

						VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
						writeDescriptorSet.dstBinding = 1;
						writeDescriptorSet.dstArrayElement = 0;
						writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
						writeDescriptorSet.descriptorCount = 1;
						writeDescriptorSet.dstSet = descriptorSet;

						vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
					}

					//material->Set("i_Depth", previousDepthBuffer);
					//material->Set("o_Depth", depthPyramid);


				}

			}
		}
	}

	void VulkanGeometryPass::ShutDown()
	{
		delete m_Data;
	}

}