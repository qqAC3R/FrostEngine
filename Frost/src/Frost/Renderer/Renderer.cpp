#include "frostpch.h"
#include "Renderer.h"

#include "Frost/Platform/Vulkan/VulkanRenderer.h"

namespace Frost
{
	RendererAPI* Renderer::s_RendererAPI;
	static RenderCommandQueue* s_CommandQueue = nullptr;

	void Renderer::Init()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::Vulkan: s_RendererAPI = new VulkanRenderer;
		}

		s_CommandQueue = new RenderCommandQueue;
		s_RendererAPI->Init();
	}

	void Renderer::ShutDown()
	{
		delete s_CommandQueue;
		s_RendererAPI->ShutDown();
	}

	void Renderer::Submit(std::function<void()> func)
	{
		s_CommandQueue->Allocate(func);
	}

	void Renderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
	{
		s_RendererAPI->Submit(mesh, transform);
	}

	void Renderer::Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
	{
		s_RendererAPI->Submit(mesh, material, transform);
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