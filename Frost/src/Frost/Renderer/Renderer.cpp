#include "frostpch.h"
#include "Renderer.h"

#include "Frost/Platform/Vulkan/VulkanRenderer.h"

namespace Frost
{
	struct RendererData
	{
		Ref<ShaderLibrary> m_ShaderLibrary;
		Ref<Texture2D> m_WhiteTexture;
	};

	RendererAPI* Renderer::s_RendererAPI = nullptr;
	static RenderCommandQueue* s_CommandQueue = nullptr;
	static RendererData* s_Data = nullptr;

	void Renderer::Init()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::Vulkan: s_RendererAPI = new VulkanRenderer;
		}

		// Init the commandQueue and Renderer data
		s_CommandQueue = new RenderCommandQueue();
		s_Data = new RendererData();

		// Init the shaders
		s_Data->m_ShaderLibrary = Ref<ShaderLibrary>::Create();
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PathTracer.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPass.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreethamSky.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBRDeffered.glsl");

		s_Data->m_WhiteTexture = Texture2D::Create("Resources/LUT/White.jpg");

		// Init the renderpasses
		s_RendererAPI->Init();
	}

	void Renderer::ShutDown()
	{
		delete s_CommandQueue;
		s_RendererAPI->ShutDown();
		s_Data->m_WhiteTexture->Destroy();
		s_Data->m_ShaderLibrary->Clear();
	}

	//void Renderer::Submit(std::function<void()> func)
	//{
	//	s_CommandQueue->Allocate(func);
	//}

	void Renderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		s_RendererAPI->Submit(mesh, transform);
	}

	void Renderer::Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
	{
		s_RendererAPI->Submit(mesh, material, transform);
	}

	Ref<ShaderLibrary> Renderer::GetShaderLibrary()
	{
		return s_Data->m_ShaderLibrary;
	}

	Ref<Texture2D> Renderer::GetWhiteLUT()
	{
		return s_Data->m_WhiteTexture;
	}

	void Renderer::ExecuteCommandBuffer()
	{
		s_CommandQueue->Execute();
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return *s_CommandQueue;
	}
}