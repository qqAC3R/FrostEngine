#include "frostpch.h"
#include "VulkanRayTracingPass.h"

#include "Frost/Platform/Vulkan/VulkanContext.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanVertexBuffer.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanIndexBuffer.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanRayTracingPipeline.h"
#include "Frost/Platform/Vulkan/RayTracing/VulkanShaderBindingTable.h"
#include "Frost/Math/Math.h"

namespace Frost
{
	static glm::vec3 PreviousCameraPosition[3];

	VulkanRayTracingPass::VulkanRayTracingPass()
		: m_Name("RayTracingPass")
	{
	}

	VulkanRayTracingPass::~VulkanRayTracingPass()
	{
	}

	static bool s_UpdateTLAS[3];
	void VulkanRayTracingPass::Init(SceneRenderPassPipeline* renderPassPipeline)
	{
		m_RenderPassPipeline = renderPassPipeline;

		m_Data.Shader = Shader::Create("assets/shader/path_tracer_demo.glsl");

		TextureSpecs imageSpec{};
		imageSpec.Width = 1600;
		imageSpec.Height = 900;
		imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
		imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.TopLevelAS[i] = TopLevelAccelertionStructure::Create();

			m_Data.SceneVertexData[i] = Buffer::Create(sizeof(uint64_t) * 1000, { BufferType::Storage, BufferType::AccelerationStructureReadOnly });
			m_Data.SceneIndexData[i] = Buffer::Create(sizeof(uint64_t) * 1000, { BufferType::Storage, BufferType::AccelerationStructureReadOnly });
			m_Data.SceneTransformData[i] = Buffer::Create(sizeof(InstanceInfo) * 1000, { BufferType::Storage, BufferType::AccelerationStructureReadOnly });


			m_Data.DisplayTexture[i] = Image2D::Create(imageSpec);

			m_Data.Descriptor[i] = Material::Create(m_Data.Shader, "RayTracingDescriptor");
			m_Data.Descriptor[i]->Set("u_Image", m_Data.DisplayTexture[i]);
			m_Data.Descriptor[i]->Set("VertexPointers", m_Data.SceneVertexData[i]);
			m_Data.Descriptor[i]->Set("IndexPointers", m_Data.SceneIndexData[i]);
			m_Data.Descriptor[i]->Set("TransformInstancePointers", m_Data.SceneTransformData[i]);
			m_Data.Descriptor[i]->UpdateVulkanDescriptor();
		}

		
		m_Data.SBT = ShaderBindingTable::Create(m_Data.Shader);


		RayTracingPipeline::CreateInfo createInfo{};
		createInfo.ShaderBindingTable = m_Data.SBT;
		createInfo.Shader = m_Data.Shader;
		m_Data.Pipeline = RayTracingPipeline::Create(createInfo);





		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			s_UpdateTLAS[i] = true;

	}

	void VulkanRayTracingPass::OnUpdate(const RenderQueue& renderQueue)
	{
		uint32_t currentFrameIndex = VulkanContext::GetSwapChain()->GetCurrentFrameIndex();
		VkCommandBuffer cmdBuf = VulkanContext::GetSwapChain()->GetRenderCommandBuffer(currentFrameIndex);
		
		Vector<std::pair<Ref<Mesh>, glm::mat4>> meshes;
		Vector<uint64_t> vertexBufferPointers;
		Vector<uint64_t> indexBufferPointers;
		Vector<InstanceInfo> transformBufferPointers;

		//for (auto& mesh : renderQueue.m_Data)
		for (uint32_t i = 0; i < renderQueue.m_DataSize; i++)
		{
			auto mesh = renderQueue.m_Data[i];

			bool isCulled = false;
#if 0

			Math::BoundingBox aabb = mesh.Mesh->GetBoundingBox();

			glm::vec3 translation, rotation, scale;
			Math::DecomposeTransform(mesh.Transform, translation, rotation, scale);

			aabb.Min.x *= scale.x;
			aabb.Min.y *= scale.y;
			aabb.Min.z *= scale.z;

			aabb.Max.x *= scale.x;
			aabb.Max.y *= scale.y;
			aabb.Max.z *= scale.z;

			glm::vec3 centerOfMesh = glm::vec3((aabb.Max.x + aabb.Min.x) / 2.0f, (aabb.Max.y + aabb.Min.y) / 2.0f, (aabb.Max.z + aabb.Min.z) / 2.0f);

			float radius = glm::distance(aabb.Min, aabb.Max) / 2.0f;
			float distanceCameraToMesh = glm::distance(renderQueue.CameraPosition, centerOfMesh);
			float tanRD = glm::tan(radius / distanceCameraToMesh);

			isCulled = tanRD > glm::tan(glm::radians(4.0f));
#endif
			if (!isCulled)
			{
				meshes.push_back(std::make_pair(mesh.Mesh, mesh.Transform));
				vertexBufferPointers.push_back(mesh.Mesh->GetVertexBuffer().As<VulkanVertexBuffer>()->GetVulkanBufferAddress());
				indexBufferPointers.push_back(mesh.Mesh->GetIndexBuffer().As<VulkanIndexBuffer>()->GetVulkanBufferAddress());

				InstanceInfo instanceInfo{};
				instanceInfo.Transform = mesh.Transform;
				instanceInfo.InverseTransform = glm::inverse(mesh.Transform);
				instanceInfo.Emittance = mesh.Mesh->GetMaterial().emission;
				instanceInfo.Roughness = mesh.Mesh->GetMaterial().roughness;
				instanceInfo.RefractionIndex = mesh.Mesh->GetMaterial().ior;


				transformBufferPointers.push_back(instanceInfo);
			}
		}
		if(meshes.size() != 0)
			m_Data.TopLevelAS[currentFrameIndex]->UpdateAccelerationStructure(meshes);

		m_Data.SceneVertexData[currentFrameIndex]->SetData((uint32_t)vertexBufferPointers.size() * sizeof(uint64_t), vertexBufferPointers.data());
		m_Data.SceneIndexData[currentFrameIndex]->SetData((uint32_t)indexBufferPointers.size() * sizeof(uint64_t), indexBufferPointers.data());
		m_Data.SceneTransformData[currentFrameIndex]->SetData((uint32_t)transformBufferPointers.size() * sizeof(InstanceInfo), transformBufferPointers.data());



		m_CameraInfo.InverseProjection = glm::inverse(renderQueue.CameraProjectionMatrix);
		m_CameraInfo.InverseView = glm::inverse(renderQueue.CameraViewMatrix);
		m_CameraInfo.RayAccumulation++;

		// Inverting the y coordonate because glm was designed for OpenGL
		m_CameraInfo.InverseProjection[1][1] *= -1;
		m_CameraInfo.CameraPosition = renderQueue.CameraPosition;


		if (PreviousCameraPosition[currentFrameIndex] != renderQueue.CameraPosition)
		{
			m_CameraInfo.RayAccumulation = -1;
			PreviousCameraPosition[currentFrameIndex] = renderQueue.CameraPosition;
		}


		// TODO: This should be made better
		if (s_UpdateTLAS[currentFrameIndex])
		{
			m_Data.Descriptor[currentFrameIndex]->Set("u_TopLevelAS", m_Data.TopLevelAS[currentFrameIndex]);
			m_Data.Descriptor[currentFrameIndex]->UpdateVulkanDescriptor();
			s_UpdateTLAS[currentFrameIndex] = false;
		}

		Ref<VulkanRayTracingPipeline> rtPipeline = m_Data.Pipeline.As<VulkanRayTracingPipeline>();
		rtPipeline->Bind();
		rtPipeline->BindVulkanPushConstant("ps_Camera", &m_CameraInfo);

		m_Data.Descriptor[currentFrameIndex]->Bind(rtPipeline->GetVulkanPipelineLayout(), GraphicsType::Raytracing);

		Ref<VulkanShaderBindingTable> rtSbt = m_Data.SBT.As<VulkanShaderBindingTable>();
		auto strideAddresses = rtSbt->GetVulkanShaderAddresses();

		vkCmdTraceRaysKHR(cmdBuf,
						  &strideAddresses[0],
						  &strideAddresses[1],
						  &strideAddresses[2],
						  &strideAddresses[3],
						  renderQueue.ViewPortWidth, renderQueue.ViewPortHeight, 1);


	}

	void VulkanRayTracingPass::OnResize(uint32_t width, uint32_t height)
	{
		TextureSpecs imageSpec{};
		imageSpec.Width = width;
		imageSpec.Height = height;
		imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
		imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;

		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.DisplayTexture[i]->Destroy();
			m_Data.DisplayTexture[i] = Image2D::Create(imageSpec);
			m_Data.Descriptor[i]->Set("u_Image", m_Data.DisplayTexture[i]);
			m_Data.Descriptor[i]->UpdateVulkanDescriptor();
		}
	}

	void VulkanRayTracingPass::ShutDown()
	{
		for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_Data.SceneVertexData[i]->Destroy();
			m_Data.SceneIndexData[i]->Destroy();
			m_Data.SceneTransformData[i]->Destroy();
			m_Data.DisplayTexture[i]->Destroy();
			m_Data.TopLevelAS[i]->Destroy();
			m_Data.Descriptor[i]->Destroy();
		}
		m_Data.SBT->Destroy();
		m_Data.Shader->Destroy();
		m_Data.Pipeline->Destroy();
	}

}