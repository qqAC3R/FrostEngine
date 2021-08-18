#include "frostpch.h"
#include "Renderer.h"

#include "Platform/Vulkan/VulkanRenderer.h"

namespace Frost
{

	RendererAPI* Renderer::s_RendererAPI = new VulkanRenderer;

}


#if 0
#include "Frost/Core/Application.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/Pipeline.h"
#include "Frost/Renderer/Framebuffer.h"

#include "Platform/Vulkan/VulkanCommandPool.h"
#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Frost/Renderer/RenderCommand.h"

#include "Platform/Vulkan/VulkanSyncObjects.h"
#include "Frost/Renderer/ShaderLibrary.h"

//#include "Frost/Renderer/RayTracing/RayTracingRenderer.h"
//#include "Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"

#include "Frost/Renderer/RayTracing/AccelerationStructures.h"
#include "Platform/Vulkan/RayTracing/VulkanAccelerationStructure.h"
#include "Frost/Renderer/RayTracing/ShaderBindingTable.h"

#include "Frost/Scene/Scene.h"

#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "Frost/Renderer/Buffers/UniformBuffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Frost/Renderer/Model.h"

#include "Frost/Utils/Timer.h"

namespace Frost
{
#define Cast(x) *(x*)

	static Ref<ShaderLibrary> m_ShaderLib;
	
	//static VulkanSwapChain m_SwapChain;
	static bool m_IsSwapChainValid;

	struct VulkanRendererData
	{
		Ref<UniformBuffer> m_CameraUBO;
		
		struct Raster
		{
			//Ref<VulkanDescriptor> m_Descriptor;
			Ref<Material> m_Material;

			Ref<VertexBuffer> m_VertexBuffer;
			Ref<IndexBuffer> m_IndexBuffer;
		};
		Raster Raster;

		struct RayTracing
		{
			Ref<TopLevelAccelertionStructure> m_TLAS;
			//Vector<Ref<BottomLevelAccelerationStructure>> m_BLAS;


			//Scope<VulkanDescriptor> m_InfoDescriptor;
			//Scope<RayTracingDescriptor> m_RTDescriptor;

			Ref<ShaderBindingTable> m_SBT;
			Ref<RayTracingPipeline> m_RayTracingPipeline;

			Ref<Material> m_Material;
			
			//Scope<VulkanRTPipeline> m_RTPipeline;
			//Scope<RayTracingRenderer> m_RTRenderer;

			//Ref<Framebuffer> m_RTFramebuffer;
			//Ref<RenderPass> m_RTRenderPass;

			Ref<Image2D> m_RayTracedImage;

		};

		RayTracing RayTracing;

		Vector<Ref<Mesh>> Meshes;

	};


	static VulkanRendererData* s_Data = nullptr;
	//static RenderCommandQueue s_RenderCommandQueue;


	static Ref<RenderPass> m_RenderPass;
	static Vector<Ref<Framebuffer>> m_Framebuffer;

	static Scope<VulkanCommandPool> m_CmdPool;
	static Scope<VulkanCommandBuffer> m_CmdBuf;

	static Ref<Pipeline> m_Pipeline;

	static VulkanFence m_Fence;
	static VulkanSemaphore m_AvailableSemapore, m_FinishedSemapore;
	
	//static Ref<Scene> m_Scene;
	



	static uint32_t ImageIndex = 0;
	static VkResult Result;
	static VkDevice Device;

	struct PushConstantData
	{
		glm::vec3 color{ 0.5f, 0.7f, 0.0f };
	};



	struct CameraMatrix
	{
		glm::mat4 Model;
		glm::mat4 View;
		glm::mat4 Projection;

		glm::mat4 InverseView;
		glm::mat4 InverseProjection;
	};

	struct PushConstant
	{
		glm::vec4 clearColor{ 0.7f, 0.2f, 0.0f, 1.0f };
		glm::vec3 lightPosition{ 10.0f, 15.0f, 8.0f };
		float lightIntensity{ 100.0f };
		int lightType = 0;
		int accumulation = 0;
		int bounces = 5;
	};

	PushConstant RayTracingPushConstant{};


	void VulkanRenderer::Init()
	{
		s_Data = new VulkanRendererData;
		Device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();
		m_ShaderLib = CreateRef<ShaderLibrary>();

		
		m_ShaderLib->Load("assets/shader/main.glsl");
		m_ShaderLib->Load("assets/shader/path_tracer_demo.glsl");

		

		m_IsSwapChainValid = true;

		{
			// COMMAND POOL
			m_CmdPool = CreateScope<VulkanCommandPool>();

			// COMMAND BUFFER
			m_CmdBuf = CreateScope<VulkanCommandBuffer>();
			m_CmdBuf->Allocate(m_CmdPool->GetCommandPool());

			// SYNC OBJECTS
			m_Fence.Create();
			m_AvailableSemapore.Create();
			m_FinishedSemapore.Create();

		}


		{
			FramebufferSpecification framebufferSpec{};
			framebufferSpec.Width = Application::Get().GetWindow().GetWidth();
			framebufferSpec.Height = Application::Get().GetWindow().GetHeight();

			framebufferSpec.Attachments = { FramebufferTextureFormat::SWAPCHAIN, FramebufferTextureFormat::Depth };
			m_RenderPass = RenderPass::Create(framebufferSpec);


		}


		{
			// Framebuffer and it's attachments	


			auto& swapChain = VulkanContext::GetSwapChain();

			size_t numberOfSwapChainImages = swapChain->GetSwapChainImages().size();
			m_Framebuffer.resize(numberOfSwapChainImages);

			for (uint32_t i = 0; i < swapChain->GetSwapChainImages().size(); i++)
			{
				FramebufferSpecification framebufferSpec{};
				framebufferSpec.Width = Application::Get().GetWindow().GetWidth();
				framebufferSpec.Height = Application::Get().GetWindow().GetHeight();

				void* swapChainImage = swapChain->GetSwapChainImages()[i];
				//void* swapChainImageFormat = (void*)swapChain->GetImageFormat();

				framebufferSpec.Attachments = {
					{ FramebufferTextureFormat::SWAPCHAIN, { swapChainImage } },
					  FramebufferTextureFormat::Depth
				};

				m_Framebuffer[i] = Framebuffer::Create(m_RenderPass->GetVulkanRenderPass(), framebufferSpec);

			}
			

			TextureSpecs imageSpec{};
			imageSpec.Width = 1600;
			imageSpec.Height = 900;
			imageSpec.Usage = { TextureSpecs::UsageSpec::Storage };
			imageSpec.Format = TextureSpecs::FormatSpec::RGBA16F;

			s_Data->RayTracing.m_RayTracedImage = Image2D::Create(imageSpec);





		}


		{	// Buffer Data

			float vertices[] = {
				//    X      Y      Z        U    V
					-1.0f, -1.0f,  0.0f,   0.0f, 0.0f,
					 1.0f, -1.0f,  0.0f,   1.0f, 0.0f,
					 1.0f,  1.0f,  0.0f,   1.0f, 1.0f,
					-1.0f,  1.0f,  0.0f,   0.0f, 1.0f,
			};
			s_Data->Raster.m_VertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));


			uint32_t indicies[] = {
				0, 1, 2,
				2, 3, 0,
			};
			s_Data->Raster.m_IndexBuffer = IndexBuffer::Create(indicies, sizeof(indicies) / sizeof(uint32_t));



			s_Data->m_CameraUBO = UniformBuffer::Create(sizeof(CameraMatrix));


			{
#if 0
				m_Scene = Frost::CreateRef<Frost::Scene>();

				MaterialMeshLayout sphereMaterial{};

				sphereMaterial.ambient = glm::vec3(1.0f, 1.0f, 1.0f);
				sphereMaterial.emission = glm::vec3(0.0f);
				sphereMaterial.illum = 3;
				sphereMaterial.roughness = glm::vec3(0.0f);
				m_Scene->AddEntity("assets/media/scenes/globe-sphere.obj", glm::translate(glm::mat4(1.0f), glm::vec3(2.5f, 0.5f, 2.0f)), sphereMaterial);

				sphereMaterial.ambient = glm::vec3(0.8f);
				sphereMaterial.illum = 2;
				m_Scene->AddEntity("assets/media/scenes/plane.obj", glm::translate(glm::mat4(1.0f), glm::vec3(4.0f, 0.0f, 1.0f)), sphereMaterial);
#endif

				//Ref<Mesh> sphereMesh = Mesh::Load("assets/media/scenes/globe-sphere.obj");
				//s_Data->Meshes.push_back(sphereMesh);
				
				Ref<Mesh> vikingMesh = Mesh::Load("assets/media/scenes/viking_room.obj");
				s_Data->Meshes.push_back(vikingMesh);

				Ref<Mesh> planeMesh = Mesh::Load("assets/media/scenes/plane.obj");
				s_Data->Meshes.push_back(planeMesh);

				//Ref<VulkanMesh> planeMesh2 = VulkanMesh::Create("assets/media/scenes/plane.obj", {});
				
				
				

				{
					Timer timer("TLAS");

					s_Data->RayTracing.m_TLAS = TopLevelAccelertionStructure::Create();
					//s_Data->RayTracing.m_TLAS->UpdateAccelerationStructure(s_Data->Meshes);

				}

				{
					//Timer timer("Update::TLAS");
					//
					//s_Data->RayTracing.m_TLAS->UpdateAccelerationStructure(s_Data->Meshes);

				}


			}

			//#Ray_tracing
			//for (auto mesh : m_Scene->GetMeshes())
			//	s_Data->RayTracing.m_BLAS.emplace_back(BottomLevelAccelerationStructure::Create(mesh));
			//
			//s_Data->RayTracing.m_TLAS = TopLevelAccelertionStructure::Create(s_Data->RayTracing.m_BLAS);



		}


		{

			{
				//s_Data->Raster.m_Descriptor = CreateRef<VulkanDescriptor>();
				//
				//
				//// RASTER DESCRIPTOR
				//Vector<VulkanDescriptorLayout> descriptorLayout;
				//
				//descriptorLayout.push_back(
				//	s_Data->RayTracing.m_RTRenderer->GetColorBufferTexture()->GetVulkanDescriptorSpec(0, { ShaderType::Fragment })
				//);
				//
				//
				//s_Data->Raster.m_Descriptor->CreateDescriptorLayout(descriptorLayout);
				//s_Data->Raster.m_Descriptor->CreateDescriptorPool(descriptorLayout);
				//s_Data->Raster.m_Descriptor->CreateDescriptorSets();
				//s_Data->Raster.m_Descriptor->UpdateLayout(descriptorLayout);

				


				s_Data->RayTracing.m_Material = Material::Create(m_ShaderLib->Get("path_tracer_demo"), "PathTracer");
				s_Data->RayTracing.m_Material->Set("image", s_Data->RayTracing.m_RayTracedImage);
				s_Data->RayTracing.m_Material->Set("CameraProperties", s_Data->m_CameraUBO);
				s_Data->RayTracing.m_Material->Set("topLevelAS", s_Data->RayTracing.m_TLAS);
				s_Data->RayTracing.m_Material->UpdateVulkanDescriptor();



				s_Data->Raster.m_Material = Material::Create(m_ShaderLib->Get("main"), "Main");
				s_Data->Raster.m_Material->Set("texSampler", s_Data->RayTracing.m_RayTracedImage);
				s_Data->Raster.m_Material->UpdateVulkanDescriptor();


			}


			{
				// RAY TRACING DESCRIPTOR

				/*
				//#SET0
				Vector<Frost::DescriptorLayout> descriptorLayout;
				// Camera (binding = 0)
				descriptorLayout.push_back({ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR });
				// Scene (binding = 1)
				descriptorLayout.push_back({ 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });
				// Verticies (binding = 2)
				descriptorLayout.push_back({ 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });
				// Indicies (binding = 3)
				descriptorLayout.push_back({ 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });
				// Materials (binding = 4)
				descriptorLayout.push_back({ 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });
				// Materials Index (binding = 5)
				descriptorLayout.push_back({ 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });
				// Textures (binding = 6)
				descriptorLayout.push_back({ 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR });

				descriptorLayout[0].SetBuffer({ &s_Data->m_CameraUBO });
				descriptorLayout[1].SetBuffer({ &m_Scene->GetInstances() });
				descriptorLayout[2].SetBuffer(m_Scene->GetVertexBuffers());
				descriptorLayout[3].SetBuffer(m_Scene->GetIndexBuffers());
				descriptorLayout[4].SetBuffer(m_Scene->GetMaterialsBuffers());
				descriptorLayout[5].SetBuffer(m_Scene->GetMatIndexBuffers());
				descriptorLayout[6].SetBuffer(m_Scene->GetTextures());

				s_Data->RayTracing.m_InfoDescriptor = Frost::CreateScope<Frost::VulkanDescriptor>();
				s_Data->RayTracing.m_InfoDescriptor->CreateDescriptorLayout(descriptorLayout);
				s_Data->RayTracing.m_InfoDescriptor->CreateDescriptorPool(descriptorLayout);
				s_Data->RayTracing.m_InfoDescriptor->CreateDescriptorSets();
				s_Data->RayTracing.m_InfoDescriptor->UpdateLayout(descriptorLayout);



				//#SET1
				s_Data->RayTracing.m_RTDescriptor = Frost::CreateScope<Frost::RayTracingDescriptor>();
				s_Data->RayTracing.m_RTDescriptor->CreateDescriptor(s_Data->RayTracing.m_AccelerationStructure->GetTLAS(),
																	s_Data->RayTracing.m_RTRenderer->GetColorBufferTexture());
				*/


			}

			
		}



		{
			{


				// TO DO: OPTION TO SELECT WHICH SHADER STAGE THE UBO SHOULD ACCESS
				//PipelineLayout pipelineInfo{};
				//pipelineInfo.SetLayoutCount = 1;
				//
				//auto descriptorLayout = s_Data->Raster.m_Descriptor->GetDescriptorLayout();
				//pipelineInfo.SetLayouts = &descriptorLayout;
				//pipelineInfo.ConstantRangeCount = 0;
				//pipelineInfo.ConstantRange = nullptr;
				//
				//
				//// GRAPHICS PIPELINE STATES
				//auto vertexShader = m_ShaderLib->Get("vert");
				//auto fragmentShader = m_ShaderLib->Get("frag");
				//
				//DepthStencilStateInfo depthStencilStateInfo{};
				//depthStencilStateInfo.DepthEnable = true;
				//depthStencilStateInfo.StencilEnable = false;
				//
				//PipelineStates pipelineStates{};
				//pipelineStates.DepthStencil = &depthStencilStateInfo;
				//pipelineStates.Multisampling = nul`lptr;



				// DRAW QUAD
				BufferLayout bufferLayout = {
					{ "a_Position", ShaderDataType::Float3 },
					{ "a_TexCoord",	ShaderDataType::Float2 } };
				
				//std::vector<Ref<Shader>> shaders = { m_ShaderLib->Get("vert"), m_ShaderLib->Get("frag") };


				Pipeline::CreateInfo pipelineCreateInfo{};
				pipelineCreateInfo.FramebufferSpecification = m_Framebuffer[0]->GetSpecification();
				pipelineCreateInfo.Shaders = m_ShaderLib->Get("main");
				pipelineCreateInfo.Descriptor = s_Data->Raster.m_Material;
				pipelineCreateInfo.RenderPass = m_RenderPass;
				pipelineCreateInfo.VertexBufferLayout = bufferLayout;
				pipelineCreateInfo.PushConstant.PushConstantRange = sizeof(glm::vec3);
				pipelineCreateInfo.PushConstant.PushConstantShaderStages = { ShaderType::Fragment };


				m_Pipeline = Pipeline::Create(pipelineCreateInfo);

				//m_Pipeline->CreateGraphicsPipeline(pipelineInfo, bufferLayout, pipelineStates, { vertexShader, fragmentShader },
				//	(VkRenderPass)m_RenderPass->GetVulkanRenderPass());
			}


			/*
			{
				auto& sbt = s_Data->RayTracing.m_SBT;
				auto& pipeline = s_Data->RayTracing.m_RTPipeline;

				sbt.LoadShader("assets/shader/rtvk/raytrace.rgen.spv", Frost::ShaderType::RayGen);
				sbt.LoadShader("assets/shader/rtvk/raytrace.rmiss.spv", Frost::ShaderType::Miss);
				sbt.LoadShader("assets/shader/rtvk/raytraceShadow.rmiss.spv", Frost::ShaderType::Miss);
				sbt.LoadShader("assets/shader/rtvk/raytrace.rchit.spv", Frost::ShaderType::ClosestHit);

				pipeline = Frost::CreateScope<Frost::VulkanRTPipeline>();
				pipeline->SetPushConstant(sizeof(PushConstant), { Frost::ShaderType::RayGen, Frost::ShaderType::ClosestHit, Frost::ShaderType::Miss });

				auto& rtDescriptor = s_Data->RayTracing.m_RTDescriptor->GetDescriptor();
				auto& infoDescriptor = s_Data->RayTracing.m_InfoDescriptor;
				pipeline->CreatePipeline({ &rtDescriptor, &infoDescriptor }, s_Data->RayTracing.m_SBT);


				sbt.CreateBindingTable(s_Data->RayTracing.m_RTPipeline->GetPipeline());
				sbt.DeleteShaders();

			}
			*/

			{

				//Ref<Shader> rayTracingShader = Shader::Create("assets/shader/rtvk/path_tracer.glsl");

				//s_Data->RayTracing.m_SBT = VulkanShaderBindingTable::Create({
				//	Shader::Create("assets/shader/rtvk/raytrace.rgen.spv",		  ShaderType::RayGen),
				//	Shader::Create("assets/shader/rtvk/raytrace.rmiss.spv",		  ShaderType::Miss),
				//	Shader::Create("assets/shader/rtvk/raytraceShadow.rmiss.spv", ShaderType::Miss),
				//	Shader::Create("assets/shader/rtvk/raytrace.rchit.spv",		  ShaderType::ClosestHit)
				//});


				s_Data->RayTracing.m_SBT = ShaderBindingTable::Create(m_ShaderLib->Get("path_tracer_demo"));
				//
				//auto& sbt = s_Data->RayTracing.VkSBT;
				//sbt = Ref<VkShaderBindingTable>::Create();
				//sbt->LoadShader("assets/cache/shader/vulkan/path_tracer_demo.glsl.cached_vulkan.rgen", Frost::ShaderType::RayGen);
				//sbt->LoadShader("assets/cache/shader/vulkan/path_tracer_demo.glsl.cached_vulkan.rmiss", Frost::ShaderType::Miss);
				//sbt->LoadShader("assets/cache/shader/vulkan/path_tracer_demo.glsl.cached_vulkan.rchit", Frost::ShaderType::ClosestHit);





				RayTracingPipeline::CreateInfo createInfo{};
				createInfo.ShaderBindingTable = s_Data->RayTracing.m_SBT;
				createInfo.Descriptor = s_Data->RayTracing.m_Material;
				createInfo.PushConstant.PushConstantRange = sizeof(PushConstant);
				createInfo.PushConstant.PushConstantShaderStages = { ShaderType::RayGen, ShaderType::Miss, ShaderType::ClosestHit };


				s_Data->RayTracing.m_RayTracingPipeline = RayTracingPipeline::Create(createInfo);
			}

		

			//vertexShader.DeleteModule();
			//fragmentShader.DeleteModule();
		}

#if 0
		struct MeshLayoutt
		{
			glm::vec3 Position;
			glm::vec3 Rotation;
		};

		Vector<MeshLayoutt> meshLayouts;
		for (uint32_t i = 0; i < 10; i++)
			meshLayouts.push_back({ {0.0f, 1.0f, 2.0f},  {0.0f, 1.0f, 2.0f}, });


		Ref<Buffer> tempBuffer = Buffer::Create(sizeof(MeshLayoutt) * 50, { BufferType::AccelerationStructureReadOnly });
		tempBuffer->SetData(sizeof(MeshLayoutt)* meshLayouts.size(), meshLayouts.data());
#endif


		
	}

	void VulkanRenderer::ShutDown()
	{
		m_Fence.Destroy();
		m_AvailableSemapore.Destroy();
		m_FinishedSemapore.Destroy();

		//m_Scene->Delete();
		for (auto& mesh : s_Data->Meshes)
			mesh->Destroy();


		m_ShaderLib->Clear();

		for(auto& framebuffer : m_Framebuffer)
			framebuffer->Destroy();

		m_RenderPass->Destroy();
		m_Pipeline->Destroy();

		s_Data->Raster.m_VertexBuffer->Destroy();
		s_Data->Raster.m_IndexBuffer->Destroy();


		s_Data->m_CameraUBO->Destroy();
		
		m_CmdPool->Destroy();


		//for (auto& blas : s_Data->RayTracing.m_BLAS)
		//	blas->Destroy();
		s_Data->RayTracing.m_Material->Destroy();
		s_Data->RayTracing.m_TLAS->Destroy();

		//s_Data->RayTracing.m_RTFramebuffer->Destroy();
		//s_Data->RayTracing.m_RTRenderPass->Destroy();
		s_Data->RayTracing.m_SBT->Destroy();
		s_Data->RayTracing.m_RayTracedImage->Destroy();


		s_Data->RayTracing.m_RayTracingPipeline->Destroy();

	}

	void VulkanRenderer::TraceRay(const Ref<RayTracingPipeline>& pipeline, const Ref<ShaderBindingTable>& sbt, const Ref<Material>& shader)
	{

		auto cmdBuf = m_CmdBuf->GetCommandBuffer();

		pipeline->Bind(cmdBuf);
		pipeline->BindVulkanPushConstant(cmdBuf, &RayTracingPushConstant);

		shader->Bind(cmdBuf, pipeline->GetVulkanPipelineLayout(), GraphicsType::Raytracing);

		auto strideAddresses = sbt->GetVulkanShaderAddresses();
		//auto strideAddresses = s_Data->RayTracing.VkSBT->GetStrideAddresses();

		vkCmdTraceRaysKHR(cmdBuf,
						 &strideAddresses[0],
						 &strideAddresses[1],
						 &strideAddresses[2],
						 &strideAddresses[3],
						 1600, 900, 1);

	}

	void VulkanRenderer::Update(void* cameraUBO, void* pushConstant)
	{
		s_Data->m_CameraUBO->SetData(cameraUBO);

		//std::vector<VkDescriptorSet> descriptors = { s_Data->RayTracing.m_RTDescriptor->GetDescriptor()->GetDescriptorSet(),
		//											s_Data->RayTracing.m_InfoDescriptor->GetDescriptorSet() };
		//VulkanRenderer::RayTrace(s_Data->RayTracing.m_RTPipeline, s_Data->RayTracing.m_SBT, descriptors, pushConstant);
	}

	VkRenderPass VulkanRenderer::GetRenderPass()
	{
		return (VkRenderPass)m_RenderPass->GetVulkanRenderPass();
	}

	

	bool VulkanRenderer::IsSwapChainValid()
	{
		return m_IsSwapChainValid;
	}

	VkCommandBuffer VulkanRenderer::GetCmdBuf()
	{
		return m_CmdBuf->GetCommandBuffer();
	}

#if 0
	void VulkanRenderer::DrawIndexed(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer, const Scope<Pipeline>& pipeline,
									 const std::initializer_list<Scope<VulkanDescriptor>>& descriptors)
	{
		//RenderCommandData drawData;
		//drawData.VertexBuffer = vertexBuffer;
		//drawData.IndexBuffer = indexBuffer;
		//drawData.Pipeline = (VkPipeline)pipeline->GetVulkanPipeline();
		//drawData.PipelineLayout = (VkPipelineLayout)pipeline->GetVulkanPipelineLayout();
		//drawData.PushConstant = nullptr;
		//
		//Vector<VkDescriptorSet> descriptorSets;
		//for (auto& descriptor : descriptors)
		//{
		//	descriptorSets.push_back(descriptor->GetDescriptorSet());
		//}
		//drawData.Descriptors = descriptorSets;
		//
		//s_RenderCommandQueue.Push(drawData, RendererType::Indexed);


	}

	void VulkanRenderer::RayTrace(const Scope<VulkanRTPipeline>& pipeline, VulkanShaderBindingTable& sbt, const Vector<VkDescriptorSet>& descriptors,
		void* data)
	{
		

		//Ref<TraceRaysCommand> cmd = CreateRef<TraceRaysCommand>();
		

		//RenderCommandData drawData;
		//drawData.Pipeline = pipeline->GetPipeline();
		//drawData.PipelineLayout = pipeline->GetPipelineLayout();
		//drawData.PushConstantInfo = pipeline->GetPushConstantInfo();
		//drawData.PushConstant = data;
		//drawData.SBT = &sbt;
		//drawData.Descriptors = descriptors;
		//
		//s_RenderCommandQueue.Push(drawData, RendererType::RayTraced);


	}
#endif

	void VulkanRenderer::WaitFence()
	{
		m_Fence.Wait();
	}

	void VulkanRenderer::BeginRenderPass(Ref<RenderPass> renderPass)
	{
		VkFramebuffer framebuffer = (VkFramebuffer)m_Framebuffer[ImageIndex]->GetRendererID();
		VkCommandBuffer cmdBuf = m_CmdBuf->GetCommandBuffer();

		VkRenderPass vkRenderPass = (VkRenderPass)m_RenderPass->GetVulkanRenderPass();


		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vkRenderPass;
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = { m_Framebuffer[ImageIndex]->GetSpecification().Width, m_Framebuffer[ImageIndex]->GetSpecification().Height };

		std::array<VkClearValue, 2> vkClearValues;
		vkClearValues[0].color = { 0.05f, 0.05f, 0.05f, 0.05f};
		vkClearValues[1].depthStencil = {1.0f, 0};


		renderPassInfo.clearValueCount = static_cast<uint32_t>(vkClearValues.size());
		renderPassInfo.pClearValues = vkClearValues.data();


		vkCmdBeginRenderPass(cmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}
	
	void VulkanRenderer::EndRenderPass()
	{
		vkCmdEndRenderPass(m_CmdBuf->GetCommandBuffer());
	}

	void VulkanRenderer::Draw()
	{
		PushConstantData pushConstantData{};

		WaitFence();
		auto [result, imageIndex] = VulkanContext::AcquireNextSwapChainImage(m_AvailableSemapore);
		Result = result;
		ImageIndex = imageIndex;



		m_CmdBuf->Begin();

		VulkanRenderer::TraceRay(s_Data->RayTracing.m_RayTracingPipeline, s_Data->RayTracing.m_SBT, s_Data->RayTracing.m_Material);


		BeginRenderPass(m_RenderPass);

		m_Pipeline->Bind(m_CmdBuf->GetCommandBuffer());
		m_Pipeline->BindVulkanPushConstant(m_CmdBuf->GetCommandBuffer(), (void*)&pushConstantData);

		s_Data->Raster.m_Material->Bind(m_CmdBuf->GetCommandBuffer(), m_Pipeline->GetVulkanPipelineLayout(), GraphicsType::Graphics);


		auto& vertexBuffer = s_Data->Raster.m_VertexBuffer;
		auto& indexBuffer = s_Data->Raster.m_IndexBuffer;
		VkCommandBuffer cmdBuf = m_CmdBuf->GetCommandBuffer();

		{

			// TODO: DrawIndexed etc.
			//RenderCommand::DrawIndexed(vertexBuffer, indexBuffer, cmdBuf);
			uint32_t count = static_cast<uint32_t>(indexBuffer->GetBufferSize() / sizeof(uint32_t));
			vertexBuffer->Bind(cmdBuf);
			indexBuffer->Bind(cmdBuf);

			vkCmdDrawIndexed(cmdBuf, count, 1, 0, 0, 0);
		}




		Application::Get().GetImGuiLayer()->Render(m_CmdBuf->GetCommandBuffer());


		EndRenderPass();
		m_CmdBuf->End();
	}

	void VulkanRenderer::PresentImage()
	{

		WaitFence();

		// Start Presenting
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_AvailableSemapore.GetSemaphore();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_FinishedSemapore.GetSemaphore();

		auto cmdBuf = m_CmdBuf->GetCommandBuffer();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuf;

		vkResetFences(Device, 1, &m_Fence.GetFence());

		VkQueue graphicsQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().GraphicsFamily.Queue;
		VkQueue presentQueue = VulkanContext::GetCurrentDevice()->GetQueueFamilies().PresentFamily.Queue;
		FROST_VKCHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_Fence.GetFence()), "Failed to submit draw command buffer!");


		auto swapChain = VulkanContext::GetSwapChain()->GetVulkanSwapChain();
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_FinishedSemapore.GetSemaphore();
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &ImageIndex;

		Result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
		{
			m_IsSwapChainValid = false;
		}
		else if (Result != VK_SUCCESS)
		{
			FROST_ASSERT(0, "Failed to present swap chain image!");
		}



	}

	VkExtent2D VulkanRenderer::GetResolution()
	{
		return VulkanContext::GetSwapChain()->GetExtent();
	}

	void VulkanRenderer::RecreatePipeline()
	{
		while (Application::Get().IsMinimized())
			Application::Get().GetWindow().OnUpdate();

		m_IsSwapChainValid = true;


		for (auto& framebuffer : m_Framebuffer)
			framebuffer->Destroy();

		m_CmdBuf->Destroy();
		m_RenderPass->Destroy();
		m_Pipeline->Destroy();

		VulkanContext::ResetSwapChain();
		//Frost::SwapChainFormats swapChainFormatsInfo;
		//swapChainFormatsInfo.Extent = m_SwapChain.GetExtent();
		//swapChainFormatsInfo.ImageFormat = m_SwapChain.GetImageFormat();
		//
		//Frost::PipelineLayout pipelineInfo{};
		//pipelineInfo.SetLayoutCount = 1;
		//
		//auto descriptorLayout = s_Data->Raster.m_Descriptor->GetDescriptorLayout();
		//pipelineInfo.SetLayouts = &descriptorLayout;
		//pipelineInfo.ConstantRangeCount = 0;
		//pipelineInfo.ConstantRange = nullptr;
		//
		//VulkanBufferLayout bufferLayout = {
		//{
		//	{ "a_Position", Frost::ShaderDataType::Float3 },
		//	{ "a_TexCoord",	Frost::ShaderDataType::Float2 }
		//},
		//VK_VERTEX_INPUT_RATE_VERTEX };
		//
		//
		//m_Pipeline = CreateScope<VulkanPipeline>(swapChainFormatsInfo);

		//Vector<Frost::AttachmentDescription> attachmentDescriptions(2);
		//attachmentDescriptions[0].Format = m_SwapChain.GetImageFormat();
		//attachmentDescriptions[0].Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		//attachmentDescriptions[0].Store = VK_ATTACHMENT_STORE_OP_STORE;
		//
		//attachmentDescriptions[1].Format = VulkanContext::GetCurrentDevice()->FindDepthFormat();
		//attachmentDescriptions[1].Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		//attachmentDescriptions[1].Store = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		//m_RenderPass->CreateRenderPass(attachmentDescriptions);

		{
			FramebufferSpecification framebufferSpec{};
			framebufferSpec.Width = Application::Get().GetWindow().GetWidth();
			framebufferSpec.Height = Application::Get().GetWindow().GetHeight();

			framebufferSpec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
			m_RenderPass = RenderPass::Create(framebufferSpec);

		}

		{
			// GRAPHICS PIPELINE STATES
			//Ref<Shader> vertexShader = Shader::Create("assets/shader/vert.spv", Frost::ShaderType::Vertex);
			//Ref<Shader> fragmentShader = Shader::Create("assets/shader/frag.spv", Frost::ShaderType::Fragment);
			//
			//DepthStencilStateInfo depthStencilStateInfo{};
			//depthStencilStateInfo.DepthEnable = true;
			//depthStencilStateInfo.StencilEnable = false;
			//
			//PipelineStates pipelineStates{};
			//pipelineStates.DepthStencil = &depthStencilStateInfo;
			//pipelineStates.Multisampling = nullptr;
			//
			//m_Pipeline->CreateGraphicsPipeline(pipelineInfo, bufferLayout, pipelineStates, { vertexShader, fragmentShader },
			//	(VkRenderPass)m_RenderPass->GetVulkanRenderPass());
			//
			//vertexShader->Destroy();
			//fragmentShader->Destroy();

		}

		{// Framebuffer
		


			//m_Framebuffer.resize(m_SwapChain.GetImages().size());
			//for (uint32_t i = 0; i < m_SwapChain.GetImages().size(); i++)
			//{
			//	FramebufferSpecification framebufferSpec{};
			//	framebufferSpec.Width = GetResolution().width;
			//	framebufferSpec.Height = GetResolution().height;
			//
			//	framebufferSpec.Attachments = {
			//		{ FramebufferTextureFormat::RGBA8, { (void*)m_SwapChain.GetImageFormat(), (void*)m_SwapChain.GetImages()[i] } },
			//		  FramebufferTextureFormat::Depth
			//	};
			//
			//
			//	m_Framebuffer[i] = Framebuffer::Create(m_RenderPass->GetVulkanRenderPass(), framebufferSpec);
			//
			//}

		}

		m_CmdBuf->Allocate(m_CmdPool->GetCommandPool());


	}

}
#endif