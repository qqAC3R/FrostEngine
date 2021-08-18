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

		inline static void BeginRenderPass(const EditorCamera& camera)
		{
			s_RendererAPI->BeginRenderPass(camera);
		}
		inline static void EndRenderPass()
		{
			s_RendererAPI->EndRenderPass();
		}


		inline static void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
		{
			s_RendererAPI->Submit(mesh, transform);
		}
		inline static void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
		{
			s_RendererAPI->Submit(mesh, material, transform);
		}

	private:
		static RendererAPI* s_RendererAPI;

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