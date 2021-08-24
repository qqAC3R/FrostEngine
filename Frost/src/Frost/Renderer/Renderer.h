#pragma once

#include "RendererAPI.h"

namespace Frost
{

	class Renderer
	{
	public:

		inline static void Init()
		{
			s_RendererAPI->Init();
		}
		inline static void ShutDown()
		{
			s_RendererAPI->ShutDown();
		}

		inline static void BeginScene(const EditorCamera& camera)
		{
			s_RendererAPI->BeginScene(camera);
		}
		inline static void EndScene()
		{
			s_RendererAPI->EndScene();
		}


		inline static void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
		{
			s_RendererAPI->Submit(mesh, transform);
		}
		inline static void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
		{
			s_RendererAPI->Submit(mesh, material, transform);
		}

		inline static void Submit(std::function<void()> func)
		{
			s_RendererAPI->Submit(func);
		}

	private:
		inline static void Update()
		{
			s_RendererAPI->Update();
		}
		
		inline static void Resize(uint32_t width, uint32_t height)
		{
			s_RendererAPI->Resize(width, height);
		}

	private:
		static RendererAPI* s_RendererAPI;
		friend class Application;
	};


#if 0
	class Material;
	class ShaderBindingTable;


	class VulkanRenderer
	{
	public:
		static void Init();
		static void ShutDown();

		//static void DrawIndexed(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer, const Scope<Pipeline>& pipeline,
		//	const std::initializer_list<Scope<VulkanDescriptor>>& descriptors);

		//static void RayTrace(const Scope<VulkanRTPipeline>& pipeline, VulkanShaderBindingTable& sbt, const Vector<VkDescriptorSet>& descriptors,
		//	void* data);
		
		static void TraceRay(const Ref<RayTracingPipeline>& pipeline, const Ref<ShaderBindingTable>& sbt, const Ref<Material>& shader);

		static VkRenderPass GetRenderPass();

		static void BeginRenderPass(Ref<RenderPass> renderPass);
		static void EndRenderPass();


		// TODO: TEMP
		static void Update(void* cameraUBO, void* pushConstant);

		static void RecreatePipeline();

		static VkExtent2D GetResolution();
		static bool IsSwapChainValid();

		static void WaitFence();

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
	private:
		
		static VkCommandBuffer GetCmdBuf();
		
		static void Draw();
		static void PresentImage();

		friend class Application;
	};
#endif

}