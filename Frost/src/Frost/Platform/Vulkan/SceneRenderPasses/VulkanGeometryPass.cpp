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
#include "Frost/Math/Math.h"

#include <imgui.h>

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
		uint64_t MaxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		m_RenderPassPipeline = renderPassPipeline;
		m_Data = new InternalData();

		m_Data->GeometryShader = Renderer::GetShaderLibrary()->Get("GeometryPassIndirectInstancedBindless");
		m_Data->LateCullShader = Renderer::GetShaderLibrary()->Get("OcclusionCulling_V3");


		GeometryDataInit(1600, 900);

		Renderer::SubmitImageToOutputImageMap("Albedo", [this]() -> Ref<Image2D>
		{
			uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
			return this->m_Data->GeometryRenderPass->GetColorAttachment(1, currentFrameIndex);
		});
	}


	void VulkanGeometryPass::InitLate()
	{
		OcclusionCullDataInit(1600, 900);
	}

	/// Geometry pass initialization
	void VulkanGeometryPass::GeometryDataInit(uint32_t width, uint32_t height)
	{
		uint64_t MaxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;


		s_RenderPassSpec =
		{
			width, height, framesInFlight,
			{
				// Normals Attachment // Attachment 0
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Albedo Attachment // Attachment 1
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// View-space Position Attachment // Attachment 2
				{
					FramebufferTextureFormat::RGBA16F, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Id Entity Attachment // Attachment 3
				{
					FramebufferTextureFormat::R32I, ImageUsage::Storage,
					OperationLoad::Clear,    OperationStore::Store,    // Color attachment
					OperationLoad::DontCare, OperationStore::DontCare, // Depth attachment
				},

				// Velocity Attachment // Attachment 4
				{
					FramebufferTextureFormat::RG16F, ImageUsage::Storage,
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
		//m_Data->GeometryRenderPass->GetColorAttachment(0, )

		// Pipeline creations
		BufferLayout bufferLayout = {
			{ "a_ModelSpaceMatrix",           ShaderDataType::Mat4   },
			{ "a_WorldSpaceMatrix",           ShaderDataType::Mat4   },
			{ "a_PreviousWorldSpaceMatrix",   ShaderDataType::Mat4   },
			{ "a_BoneInformationBDA",         ShaderDataType::UInt64 },
			{ "a_MaterialIndexGlobalOffset",  ShaderDataType::UInt   },
			{ "a_EntityID",                   ShaderDataType::UInt   }
			//{ "IsMeshVisible",                ShaderDataType::UInt   },
			//{ "a_Padding0",                   ShaderDataType::UInt   },
			//{ "a_Padding1",                   ShaderDataType::UInt   },
			//{ "a_Padding2",                   ShaderDataType::UInt   },
		};
		bufferLayout.m_InputType = InputType::Instanced;

		Pipeline::CreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.Shader = m_Data->GeometryShader;
		pipelineCreateInfo.UseDepthTest = true;
		pipelineCreateInfo.UseDepthWrite = true;
		pipelineCreateInfo.UseStencil = false;
		pipelineCreateInfo.Cull = CullMode::Back;
		//pipelineCreateInfo.AlphaBlending = true;
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
				indirectCmdBuffer.DeviceBuffer = BufferDevice::Create(sizeof(VkDrawIndexedIndirectCommand) * MaxCountMeshes, { BufferUsage::Storage, BufferUsage::Indirect });

				indirectCmdBuffer.HostBuffer.Allocate(sizeof(VkDrawIndexedIndirectCommand) * MaxCountMeshes);
			}

			/// Instance data storage buffer
			m_Data->MaterialSpecs.resize(framesInFlight);
			for (uint32_t i = 0; i < m_Data->MaterialSpecs.size(); i++)
			{
				auto& instanceSpec = m_Data->MaterialSpecs[i];

				// Allocating a heap block
				instanceSpec.DeviceBuffer = BufferDevice::Create(sizeof(MaterialData) * MaxCountMeshes, { BufferUsage::Storage });
				instanceSpec.HostBuffer.Allocate(sizeof(MaterialData) * MaxCountMeshes);

				// Setting the storage buffer into the descriptor
				m_Data->GeometryDescriptor[i]->Set("u_MaterialUniform", instanceSpec.DeviceBuffer);
			}

			/// Global Instaced Vertex Buffer
			m_Data->GlobalInstancedVertexBuffer.resize(framesInFlight);
			for (uint32_t i = 0; i < m_Data->GlobalInstancedVertexBuffer.size(); i++)
			{
				auto& instancdVertexBuffer = m_Data->GlobalInstancedVertexBuffer[i];

				// Allocating a heap block
				instancdVertexBuffer.DeviceBuffer = BufferDevice::Create(sizeof(MeshInstancedVertexBuffer) * MaxCountMeshes, { BufferUsage::Vertex, BufferUsage::Storage });
				instancdVertexBuffer.HostBuffer.Allocate(sizeof(MeshInstancedVertexBuffer) * MaxCountMeshes);
			}
		}

	}

	void VulkanGeometryPass::OcclusionCullDataInit(uint32_t width, uint32_t height)
	{
		uint64_t MaxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();


		//m_Data->ComputeShaderPushConstant.DepthPyramidSize.x = width;
		//m_Data->ComputeShaderPushConstant.DepthPyramidSize.y = height;

		//////////////////////////////////////////////
		// LATE CULLING //////////////////////////////
		//////////////////////////////////////////////
		{
			if (!m_Data->LateCullPipeline)
			{
				ComputePipeline::CreateInfo computePipelineCreateInfo{};
				computePipelineCreateInfo.Shader = m_Data->LateCullShader;
				m_Data->LateCullPipeline = ComputePipeline::Create(computePipelineCreateInfo);

				//m_Data->DebugDeviceBuffer = BufferDevice::Create(sizeof(glm::mat4) * 1024, { BufferUsage::Storage });

				m_Data->MeshSpecs.resize(framesInFlight);
				for (uint32_t i = 0; i < framesInFlight; i++)
				{
					m_Data->MeshSpecs[i].DeviceBuffer = BufferDevice::Create(sizeof(MeshData_OC) * MaxCountMeshes, { BufferUsage::Storage });
					m_Data->MeshSpecs[i].HostBuffer.Allocate(sizeof(MeshData_OC) * MaxCountMeshes);
				}
			}

			m_Data->LateCullDescriptor.resize(framesInFlight);
			for (uint32_t i = 0; i < m_Data->LateCullDescriptor.size(); i++)
			{
				auto& computeDescriptor = m_Data->LateCullDescriptor[i];
				if (!computeDescriptor)
					computeDescriptor = Material::Create(m_Data->LateCullShader, "OcclusionCulling");

				auto& computeVulkanDescriptor = m_Data->LateCullDescriptor[i].As<VulkanMaterial>();
				VkDescriptorSet descriptorSet = computeVulkanDescriptor->GetVulkanDescriptorSet(0);

				int32_t previousFrameIndex = (int32_t)i - 1;
				if (previousFrameIndex < 0)
					previousFrameIndex = Renderer::GetRendererConfig().FramesInFlight - 1;


				computeDescriptor->Set("InstancedVertexBuffer", m_Data->GlobalInstancedVertexBuffer[i].DeviceBuffer);
				computeDescriptor->Set("MeshSpecs", m_Data->MeshSpecs[i].DeviceBuffer);

				auto lastFrameDepthPyramid = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->DepthPyramid[previousFrameIndex];
				Ref<VulkanImage2D> vulkanLastDepthPyramid = lastFrameDepthPyramid.As<VulkanImage2D>();

				VkSampler lastFrameDepthPyramidSampler = m_RenderPassPipeline->GetRenderPassData<VulkanPostFXPass>()->HZBNearestSampler[previousFrameIndex];

				// HZB with custum linear sampler (exclusive for SSR)
				VkDescriptorImageInfo imageDescriptorInfo{};
				imageDescriptorInfo.imageView = vulkanLastDepthPyramid->GetVulkanImageView();
				imageDescriptorInfo.imageLayout = vulkanLastDepthPyramid->GetVulkanImageLayout();
				imageDescriptorInfo.sampler = lastFrameDepthPyramidSampler;

				VkWriteDescriptorSet writeDescriptorSet{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				writeDescriptorSet.dstBinding = 2; // layout(binding = 2) uniform sampler2D u_DepthPyramid;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSet.pImageInfo = &imageDescriptorInfo;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;

				vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, 0);

				computeVulkanDescriptor->UpdateVulkanDescriptorIfNeeded();
			}
		}
	}


	void VulkanGeometryPass::OnUpdate(const RenderQueue& renderQueue)
	{
		// If we have 0 meshes, we shouldnt render this pass
		if (renderQueue.GetQueueSize() == 0) return;


		//GetOcclusionQueryResults();

		VulkanRenderer::BeginTimeStampPass("Geometry Pass");
		//GeometryPrepareIndirectData(renderQueue);
		GeometryPrepareIndirectDataWithInstacing(renderQueue);
		GeometryUpdateWithInstancing(renderQueue);
		VulkanRenderer::EndTimeStampPass("Geometry Pass");
	}


	bool Within(float min, float value, float max)
	{
		return value >= min && value <= max;
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
	static glm::mat4 s_PreviousViewProjectioMatrix = glm::mat4(1.0f);
	static glm::mat4 s_CurrentViewProjectioMatrix = glm::mat4(1.0f);
	static uint64_t s_TotalSubmeshSubmitted = 0;
#if 0
	void VulkanGeometryPass::ObjectCullingPrepareData(const RenderQueue& renderQueue)
	{
		for (auto& [handle, groupedMeshes] : s_GroupedMeshesCached)
		{
			Ref<MeshAsset> meshAsset = groupedMeshes[0].Mesh->GetMeshAsset();
			const Vector<Submesh>& submeshes = meshAsset->GetSubMeshes();

			// Set up the instanced vertex buffer (per submesh, per instance)
			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); submeshIndex++)
			{
				MeshInstancedVertexBuffer meshInstancedVertexBuffer{};
				uint32_t meshInstanceNr = 0;
				for (auto& meshInstance : groupedMeshes)
				{
					glm::mat4 modelMatrix = meshInstance.Transform * meshInstance.Mesh->GetSkeletalSubmeshes()[submeshIndex].Transform;

					// TEMP!!!!
					const Submesh& submesh = meshInstance.Mesh->GetMeshAsset()->GetSubMeshes()[submeshIndex];

					if (!mFreezeCameraFrustum)
						m_LastViewProjMatrix = s_CurrentViewProjectioMatrix;

					glm::mat4 worldSpaceMatrix = m_LastViewProjMatrix * modelMatrix;

					glm::vec3 scaleFactor = glm::vec3(
						submesh.BoundingBox.Max.x - submesh.BoundingBox.Min.x,
						submesh.BoundingBox.Max.y - submesh.BoundingBox.Min.y,
						submesh.BoundingBox.Max.z - submesh.BoundingBox.Min.z
					);



					// Use our Min Max to define eight corners
					glm::vec4 corners[8] = {
						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Min.z, 1.0}, // x y z
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Min.z, 1.0}, // X y z
						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Min.z, 1.0}, // x Y z
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Min.z, 1.0}, // X Y z

						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Max.z, 1.0}, // x y Z
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Max.z, 1.0}, // X y Z
						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Max.z, 1.0}, // x Y Z
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Max.z, 1.0}, // X Y Z
					};

					bool inside = false;

					for (size_t corner_idx = 0; corner_idx < 8; corner_idx++) {
						// Transform vertex
						glm::vec4 corner = worldSpaceMatrix * corners[corner_idx];
						// Check vertex against clip space bounds
						inside = inside ||
							Within(-corner.w, corner.x, corner.w) &&
							Within(-corner.w, corner.y, corner.w) &&
							Within(0.0f, corner.z, corner.w);
					}
					//currentIndirectMeshData->SubMeshIndexCulled.push_back(inside);

				}
			}
		}
	}
#endif


	struct Plane
	{
		glm::vec3 Normal = { 0.0f, 1.0f, 0.0f };// unit vector
		float Distance = 0.0f;// distance from origin to the nearest point in the plane

		Plane() = default;

		Plane(const glm::vec3& distance, const glm::vec3& normal)
			: Normal(glm::normalize(normal)), Distance(glm::dot(Normal, distance))
		{}

		float GetSignedDistanceToPlane(const glm::vec3& point) const
		{
			return glm::dot(Normal, point) - Distance;
		}
	};

	struct Frustum
	{
		Plane TopFace;
		Plane BottomFace;

		Plane RightFace;
		Plane LeftFace;

		Plane FarFace;
		Plane NearFace;
	};

	struct AABB
	{
		glm::vec3 Center{ 0.0f, 0.0f, 0.0f };
		glm::vec3 Extents{ 0.0f, 0.0f, 0.0f };

		AABB(const glm::vec3& min, const glm::vec3& max)
			: Center{ (max + min) * 0.5f },
			Extents{ max.x - Center.x, max.y - Center.y, max.z - Center.z }
		{}

		AABB(const glm::vec3& inCenter, float iI, float iJ, float iK)
			: Center{ inCenter }, Extents{ iI, iJ, iK }
		{}

		// https://gdbooks.gitbooks.io/3dcollisions/content/Chapter2/static_aabb_plane.html
		bool IsOnOrForwardPlane(const Plane& plane)
		{
			// Compute the projection interval radius of b onto L(t) = b.c + t * p.n
			const float r = Extents.x * std::abs(plane.Normal.x) +
				            Extents.y * std::abs(plane.Normal.y) +
				            Extents.z * std::abs(plane.Normal.z);

			return -r <= plane.GetSignedDistanceToPlane(Center);
		}

		bool IsOnFrustum(const Frustum& camFrustum, const glm::mat4& modelMatrix)
		{
			//Get global scale thanks to our transform
			const glm::vec3 globalCenter{ modelMatrix * glm::vec4(Center, 1.0f) };
			
			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(modelMatrix, translation, rotation, scale);

			glm::vec3 upVector = glm::quat(rotation) * glm::vec3(0.0f, 1.0f, 0.0f);
			glm::vec3 rightVector = glm::quat(rotation) * glm::vec3(1.0f, 0.0f, 0.0f);
			glm::vec3 frontVector = glm::quat(rotation) * glm::vec3(0.0f, 0.0f, -1.0f);

			// Scaled orientation
			const glm::vec3 right = rightVector * Extents.x * scale;
			const glm::vec3 up = upVector * Extents.y * scale;
			const glm::vec3 forward = frontVector * Extents.z * scale;

			const float newIi = std::abs(glm::dot(glm::vec3{ 1.0f, 0.0f, 0.0f }, right)) +
				                std::abs(glm::dot(glm::vec3{ 1.0f, 0.0f, 0.0f }, up)) +
				                std::abs(glm::dot(glm::vec3{ 1.0f, 0.0f, 0.0f }, forward));

			const float newIj = std::abs(glm::dot(glm::vec3{ 0.0f, 1.0f, 0.0f }, right)) +
				                std::abs(glm::dot(glm::vec3{ 0.0f, 1.0f, 0.0f }, up)) +
				                std::abs(glm::dot(glm::vec3{ 0.0f, 1.0f, 0.0f }, forward));

			const float newIk = std::abs(glm::dot(glm::vec3{ 0.0f, 0.0f, 1.0f }, right)) +
				                std::abs(glm::dot(glm::vec3{ 0.0f, 0.0f, 1.0f }, up)) +
				                std::abs(glm::dot(glm::vec3{ 0.0f, 0.0f, 1.0f }, forward));

			AABB globalAABB(globalCenter, newIi, newIj, newIk);

			return (globalAABB.IsOnOrForwardPlane(camFrustum.LeftFace) &&
				    globalAABB.IsOnOrForwardPlane(camFrustum.RightFace) &&
				    globalAABB.IsOnOrForwardPlane(camFrustum.TopFace) &&
				    globalAABB.IsOnOrForwardPlane(camFrustum.BottomFace) &&
				    globalAABB.IsOnOrForwardPlane(camFrustum.NearFace) &&
				    globalAABB.IsOnOrForwardPlane(camFrustum.FarFace));
		};
	};
	
	static Frustum CreateFrustum(const RenderQueue& renderQueue)
	{
		Ref<EditorCamera> sceneCamera = renderQueue.m_Camera.As<EditorCamera>();

		float screenAspectRatio = (float)renderQueue.ViewPortWidth / (float)renderQueue.ViewPortHeight;
		glm::vec3 frontVector = sceneCamera->GetForwardDirection();
		glm::vec3 rightVector = glm::normalize(glm::cross(frontVector, glm::vec3(0.0f, 1.0f, 0.0f)));
		glm::vec3 upVector = glm::normalize(glm::cross(rightVector, frontVector));
		//glm::vec3 rightVector = sceneCamera->GetRightDirection();
		//glm::vec3 upVector = sceneCamera->GetUpDirection();
		glm::vec3 cameraPosition = sceneCamera->GetPosition();

		//glm::vec3 CamRight = glm::normalize(glm::cross(mCameraFront, glm::vec3(0.0f, 1.0f, 0.0f)));
		//glm::vec3 CamUp = glm::normalize(glm::cross(CamRight, mCameraFront));

		Frustum frustum;
		const float halfHeightSide = sceneCamera->GetFarClip() * tanf(glm::radians(sceneCamera->GetCameraFOV() * 0.5f));
		const float halfWidthSide = halfHeightSide * screenAspectRatio;
		const glm::vec3 farPlaneDirection = sceneCamera->GetFarClip() * frontVector;

		frustum.NearFace = { cameraPosition + sceneCamera->GetNearClip() * frontVector, frontVector };
		frustum.FarFace = { cameraPosition + farPlaneDirection, -frontVector };
		
		frustum.RightFace = { cameraPosition, glm::cross(farPlaneDirection - rightVector * halfWidthSide, upVector) };
		frustum.LeftFace = { cameraPosition,  glm::cross(upVector, farPlaneDirection + rightVector * halfWidthSide) };

		frustum.TopFace = { cameraPosition,      glm::cross(rightVector, farPlaneDirection - upVector * halfHeightSide) };
		frustum.BottomFace = { cameraPosition,   glm::cross(farPlaneDirection + upVector * halfHeightSide, rightVector) };

		return frustum;
	}

	static bool ComputeFrustumCulling(const RenderQueue& renderQueue, const glm::mat4& modelMatrix, const Math::BoundingBox& boundingBox)
	{
		Frustum cameraFrustum = CreateFrustum(renderQueue);

		glm::vec3 translation, rotation, scale;
		Math::DecomposeTransform(modelMatrix, translation, rotation, scale);

		AABB meshAABB(boundingBox.Min, boundingBox.Max);
		return meshAABB.IsOnFrustum(cameraFrustum, modelMatrix);
	}

	void VulkanGeometryPass::GeometryPrepareIndirectDataWithInstacing(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);

		MeshData_OC meshdataForOcclusionCulling{};

		// Get the needed matricies from the camera
		//glm::mat4 projectionMatrix = renderQueue.CameraProjectionMatrix;
		//projectionMatrix[1][1] *= -1; // GLM uses opengl style of rendering, where the y coordonate is inverted
		s_PreviousViewProjectioMatrix = s_CurrentViewProjectioMatrix;
		s_CurrentViewProjectioMatrix = renderQueue.m_Camera->GetViewProjectionVK();

		/*
			Each mesh might have a set of submeshes which are sent to render individualy.
			We dont need them when we render them indirectly (because the gpu renders all the submeshes automatically - `multidraw`),
			instead we just need to know the `start index` of every mesh in the `VkDrawIndexedIndirectCommand` buffer.
			(TODO: This explanation is outdated, now we are using `instanced indirect multidraw rendering` :D)
		*/
		s_GeometryMeshIndirectData.clear();
		s_GroupedMeshesCached.clear();
		s_TotalSubmeshSubmitted = 0;

		// Allocate all the neccesary array buffers before, so we won't waste cpus cycles on reallocating memory
		for (auto& [meshAssetUUID, instanceCount] : renderQueue.m_MeshInstanceCount)
		{
			s_GroupedMeshesCached[meshAssetUUID].reserve(instanceCount);
		}
		s_GeometryMeshIndirectData.reserve(s_GroupedMeshesCached.size());


		for (uint32_t i = 0; i < renderQueue.GetQueueSize(); i++)
		{
			// Get the mesh
			auto mesh = renderQueue.m_Data[i].Mesh;

			s_GroupedMeshesCached[mesh->GetMeshAsset()->Handle].push_back({ mesh.Raw(), renderQueue.m_Data[i].Transform, i, renderQueue.m_Data[i].EntityID});
		}

		//ObjectCullingPrepareData(renderQueue);


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
			currentIndirectMeshData = &s_GeometryMeshIndirectData.emplace_back();

			// We are checking `.size() > 1` and also `.size() - 2` because we've already pushed_back and element before, so we should go 2 elements behind instead of one
			if (s_GeometryMeshIndirectData.size() > 1)
				lastIndirectMeshData = &s_GeometryMeshIndirectData[s_GeometryMeshIndirectData.size() - 2];

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
					meshInstancedVertexBuffer.WorldSpaceMatrix = s_CurrentViewProjectioMatrix * modelMatrix;
					meshInstancedVertexBuffer.PreviousWorldSpaceMatrix = s_PreviousViewProjectioMatrix * modelMatrix;
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

					

#if 1
					// TEMP!!!!
					const Submesh& submesh = meshInstance.Mesh->GetMeshAsset()->GetSubMeshes()[submeshIndex];
					
					

#if 0
					// Use our Min Max to define eight corners
					glm::vec4 corners[8] = {
						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Min.z, 1.0},
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Min.z, 1.0},
						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Min.z, 1.0},
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Min.z, 1.0},

						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Max.z, 1.0},
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Min.y, submesh.BoundingBox.Max.z, 1.0},
						{submesh.BoundingBox.Min.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Max.z, 1.0},
						{submesh.BoundingBox.Max.x, submesh.BoundingBox.Max.y, submesh.BoundingBox.Max.z, 1.0},
					};

					bool inside = false;
					for (size_t cornerIndex = 0; cornerIndex < 8; cornerIndex++) {
						// Transform vertex
						glm::vec4 corner = worldSpaceMatrix * corners[cornerIndex];
						// Check vertex against clip space bounds
						inside = inside ||
							(Within(-corner.w, corner.x, corner.w) &&
							Within(-corner.w, corner.y, corner.w) &&
							Within(0.0f, corner.z, corner.w));
					}
#endif
					
					bool inside = ComputeFrustumCulling(renderQueue, modelMatrix, submesh.BoundingBox);
					meshInstancedVertexBuffer.ModelSpaceMatrix[3][3] = (float)inside;

#if 0
					// Transform the AABB into 2D screen space
					glm::vec4 minScreenSpace = meshInstancedVertexBuffer.WorldSpaceMatrix * glm::vec4(submesh.BoundingBox.Min, 1.0f);
					glm::vec4 maxScreenSpace = meshInstancedVertexBuffer.WorldSpaceMatrix * glm::vec4(submesh.BoundingBox.Max, 1.0f);
					minScreenSpace /= minScreenSpace.w;
					maxScreenSpace /= maxScreenSpace.w;

					glm::vec4 minViewPos = renderQueue.m_Camera->GetViewMatrix() * glm::vec4(submesh.BoundingBox.Min, 1.0f);
					glm::vec4 maxViewPos = renderQueue.m_Camera->GetViewMatrix() * glm::vec4(submesh.BoundingBox.Max, 1.0f);

					// clip objects behind near plane
#if 0
					//minScreenSpace.z *= glm::step(minViewPos.z, renderQueue.m_Camera->GetNearClip());
					//maxScreenSpace.z *= glm::step(maxViewPos.z, renderQueue.m_Camera->GetNearClip());

					//minScreenSpace.z = glm::clamp(minScreenSpace.z, 0.0f, 1.0f);
					//maxScreenSpace.z = glm::clamp(maxScreenSpace.z, 0.0f, 1.0f);
#endif

					// Convert to pixel coordinates
					glm::vec2 uvMin = (glm::clamp(glm::vec2(minScreenSpace), glm::vec2(-1.0), glm::vec2(1.0)) * glm::vec2(0.5) + glm::vec2(0.5));
					glm::vec2 uvMax = (glm::clamp(glm::vec2(maxScreenSpace), glm::vec2(-1.0), glm::vec2(1.0)) * glm::vec2(0.5) + glm::vec2(0.5));

					glm::vec2 screenPosMin = uvMin * glm::vec2(renderQueue.ViewPortWidth, renderQueue.ViewPortHeight);
					glm::vec2 screenPosMax = uvMax * glm::vec2(renderQueue.ViewPortWidth, renderQueue.ViewPortHeight);

#if 0
					//if (minScreenSpace.z > 1.0 && maxScreenSpace.z > 1.0)
					//{
					//	screenPosMin = glm::vec2(0.0f);
					//	screenPosMax = glm::vec2(0.0f);
					//}
#endif

					// Calculate the number of pixels
					glm::vec2 screenRect = glm::abs(glm::vec2(screenPosMax - screenPosMin));
					screenRect.x = glm::clamp(screenRect.x, 0.0f, (float)renderQueue.ViewPortWidth);
					screenRect.y = glm::clamp(screenRect.y, 0.0f, (float)renderQueue.ViewPortHeight);
					int32_t pixels = int32_t(abs(screenRect.x) * abs(screenRect.y));

					//FROST_CORE_INFO("{0}, {1}", screenRect.x, screenRect.y);
#endif

					
					m_Data->GlobalInstancedVertexBuffer[currentFrameIndex].HostBuffer.Write((void*)&meshInstancedVertexBuffer, sizeof(MeshInstancedVertexBuffer), instanceVertexOffset);
					instanceVertexOffset += sizeof(MeshInstancedVertexBuffer);

					// Setting up `Mesh data` for the occlusion culling compute shader
					meshdataForOcclusionCulling.Transform = modelMatrix;
					meshdataForOcclusionCulling.AABB_Min = glm::vec4(submesh.BoundingBox.Min, 1.0f);
					meshdataForOcclusionCulling.AABB_Max = glm::vec4(submesh.BoundingBox.Max, 1.0f);
					m_Data->MeshSpecs[currentFrameIndex].HostBuffer.Write(
						(void*)&meshdataForOcclusionCulling,
						sizeof(MeshData_OC),
						s_TotalSubmeshSubmitted * sizeof(MeshData_OC)
					);
					s_TotalSubmeshSubmitted++;
					meshInstanceNr++;

					//glm::vec3 translation, rotation, scale;
					//Math::DecomposeTransform(modelMatrix, translation, rotation, scale);
					//
					// 
					//glm::vec3 scaleFactor = glm::vec3(
					//	submesh.BoundingBox.Max.x - submesh.BoundingBox.Min.x,
					//	submesh.BoundingBox.Max.y - submesh.BoundingBox.Min.y,
					//	submesh.BoundingBox.Max.z - submesh.BoundingBox.Min.z
					//);
					// 
					//glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation) * glm::scale(glm::mat4(1.0f), scaleFactor * scale);
					//Renderer::SubmitWireframeMesh(MeshAsset::GetDefaultMeshes().Cube, transform, glm::vec4(0.8f, 0.5f, 0.4f, 1.0f), 2.0f);
#endif
				}
			}

			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); submeshIndex++)
			{
				const Submesh& submesh = submeshes[submeshIndex];

				// Submit the submesh into the cpu buffer
				VkDrawIndexedIndirectCommand indirectCmdBuf{};
				indirectCmdBuf.firstIndex = submesh.BaseIndex;
				indirectCmdBuf.indexCount = submesh.IndexCount;
#if 0
				indirectCmdBuf.firstIndex = meshAsset->GetSubMeshesLOD(1)[submeshIndex].BaseIndex;
				indirectCmdBuf.indexCount = meshAsset->GetSubMeshesLOD(1)[submeshIndex].IndexCount;
#endif

				// Vertexoffset:  basically adds what offset you want to the indices before they are actually rendered.
				// TODO: Previously, each index was using its own scale, from 0 to n
				// TODO: Now, the index buffer is global, so we do not need to add any offset to the indices
				//indirectCmdBuf.vertexOffset = submesh.BaseVertex;
				indirectCmdBuf.vertexOffset = 0;

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

				// Frustum Cull
				//if (!currentIndirectMeshData->SubMeshIndexCulled[submeshIndex])
				//{
				//	indirectCmdBuf.indexCount = 0;
				//	indirectCmdBuf.instanceCount = 1;
				//	indirectCmdBuf.firstIndex = 0;
				//	indirectCmdBuf.vertexOffset = 0;
				//	indirectCmdBuf.firstInstance = 0;
				//}

				m_Data->IndirectCmdBuffer[currentFrameIndex].HostBuffer.Write((void*)&indirectCmdBuf, sizeof(VkDrawIndexedIndirectCommand), indirectCmdsOffset);
				indirectCmdsOffset += sizeof(VkDrawIndexedIndirectCommand);
			}
		}

		//FROST_CORE_INFO("FINISH!!");

		//VulkanRenderer::BeginTimeStampPass("Occlusion Culling");
		OcclusionCullUpdate(renderQueue, indirectCmdsOffset);
		//VulkanRenderer::EndTimeStampPass("Occlusion Culling");



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


		auto meshSpecificationBuffer = m_Data->MeshSpecs[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();
		void* meshSpecificationBufferPointer = m_Data->MeshSpecs[currentFrameIndex].HostBuffer.Data;
		meshSpecificationBuffer->SetData(s_TotalSubmeshSubmitted * sizeof(MeshData_OC), meshSpecificationBufferPointer);
	}

	void VulkanGeometryPass::GeometryUpdateWithInstancing(const RenderQueue& renderQueue)
	{
		// Getting all the needed information
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		uint64_t MaxCountMeshes = Renderer::GetRendererConfig().MaxMeshCount_GeometryPass;
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		Ref<Framebuffer> framebuffer = m_Data->GeometryRenderPass->GetFramebuffer(currentFrameIndex);
		Ref<VulkanPipeline> vulkanPipeline = m_Data->GeometryPipeline.As<VulkanPipeline>();
		auto vulkanIndirectCmdBuffer = m_Data->IndirectCmdBuffer[currentFrameIndex].DeviceBuffer.As<VulkanBufferDevice>();

		// Setting up the geometry pass push constant buffer
		m_GeometryPushConstant.ViewMatrix = renderQueue.CameraViewMatrix;

		auto GetJitter = [](const uint64_t frameCount, float screenWidth, float screenHeight) {
			auto HaltonSeq = [](int32_t prime, int32_t idx) {
				float r = 0;
				float f = 1;
				while (idx > 0) {
					f /= prime;
					r += f * (idx % prime);
					idx /= prime;
				}
				return r;
			};
			float u = HaltonSeq(2, (frameCount % 8) + 1) - 0.5f;
			float v = HaltonSeq(3, (frameCount % 8) + 1) - 0.5f;
			return glm::vec2(u, v) * glm::vec2(1.0 / screenWidth, 1. / screenHeight) * 2.f;
		};
		uint64_t currentFrameCount = Renderer::GetFrameCount();
		m_GeometryPushConstant.JitterCurrent = GetJitter(currentFrameCount, renderQueue.ViewPortWidth, renderQueue.ViewPortHeight);
		m_GeometryPushConstant.JitterPrevious = GetJitter(currentFrameCount - 1, renderQueue.ViewPortWidth, renderQueue.ViewPortHeight);
		
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
			//meshAsset->GetIndexBuffer()->Bind(); // TODO: Also this is changed
			meshAsset->GetSubmeshIndexBuffer()->Bind(); // TODO: Also this is changed
			//meshAsset->GetGlobalSubmeshIndexBuffer()->Bind(); // TODO: Also this is changed

			m_GeometryPushConstant.VertexBufferBDA = meshAsset->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress();
			m_GeometryPushConstant.IsAnimated = static_cast<uint32_t>(meshAsset->IsAnimated());

			vulkanPipeline->BindVulkanPushConstant("u_PushConstant", (void*)&m_GeometryPushConstant);

#if 1
			uint32_t offset = indirectPerMeshData.CmdOffset * sizeof(VkDrawIndexedIndirectCommand);
			vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, indirectPerMeshData.SubmeshCount, sizeof(VkDrawIndexedIndirectCommand));

#else
			Vector<Submesh> submeshes = meshAsset->GetSubMeshes();
			for (uint32_t submeshIndex = 0; submeshIndex < submeshes.size(); submeshIndex++)
			{
				//auto& submesh = submeshes[i];
				uint32_t submeshCount = indirectPerMeshData.SubmeshCount;
				uint32_t offset = indirectPerMeshData.CmdOffset * sizeof(VkDrawIndexedIndirectCommand) + (sizeof(VkDrawIndexedIndirectCommand) * submeshIndex);

				//if (std::find(indirectPerMeshData.SubMeshIndexCulled.begin(), indirectPerMeshData.SubMeshIndexCulled.end(), (uint64_t)submeshIndex) != indirectPerMeshData.SubMeshIndexCulled.end())
				if (indirectPerMeshData.SubMeshIndexCulled[submeshIndex])
				{
					vkCmdDrawIndexedIndirect(cmdBuf, vulkanIndirectCmdBuffer->GetVulkanBuffer(), offset, 1, sizeof(VkDrawIndexedIndirectCommand));
				}
			}
#endif
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
				meshData.submesh.BoundingBox_Min = glm::vec4(submesh.BoundingBox.Min, 1.0f);
				meshData.submesh.BoundingBox_Max = glm::vec4(submesh.BoundingBox.Max, 1.0f);
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
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
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

	struct OcclusionCullingPushConstant
	{
		glm::mat4 ProjectionMatrix;
		glm::mat4 ViewMatrix;
		uint32_t NumberOfSubmeshes;
		float CameraNearClip;
	} s_PushConstant_OcclusionCulling;

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


			//s_PushConstant_OcclusionCulling.ViewProjectionMatrix = m_LastViewProjMatrix;
			s_PushConstant_OcclusionCulling.ProjectionMatrix = renderQueue.m_Camera->GetProjectionMatrix();
			s_PushConstant_OcclusionCulling.ProjectionMatrix[1][1] *= -1;

			s_PushConstant_OcclusionCulling.ViewMatrix = renderQueue.m_Camera->GetViewMatrix();
			s_PushConstant_OcclusionCulling.NumberOfSubmeshes = (indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand));
			s_PushConstant_OcclusionCulling.CameraNearClip = renderQueue.m_Camera->GetNearClip();

#if 0
			m_Data->ComputeShaderPushConstant.DepthPyramidSize.z = indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand);

			m_Data->ComputeShaderPushConstant.CamFar = renderQueue.m_Camera->GetFarClip();
			m_Data->ComputeShaderPushConstant.CamNear = renderQueue.m_Camera->GetNearClip();
			m_Data->ComputeShaderPushConstant.ViewMatrix = renderQueue.m_Camera->GetViewMatrix();
			m_Data->ComputeShaderPushConstant.ProjectionMaxtrix = renderQueue.m_Camera->GetProjectionMatrix();
			m_Data->ComputeShaderPushConstant.ProjectionMaxtrix[1][1] *= -1;
#endif

			vulkanComputePipeline->BindVulkanPushConstant(cmdBuf, "u_PushConstant", &s_PushConstant_OcclusionCulling);

			uint32_t workGroupsX = std::ceil((indirectCmdsOffset / sizeof(VkDrawIndexedIndirectCommand)) / 64.0f);
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
			return this->m_Data->GeometryRenderPass->GetColorAttachment(1, currentFrameIndex);
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