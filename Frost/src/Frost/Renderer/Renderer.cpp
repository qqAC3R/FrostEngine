#include "frostpch.h"
#include "Renderer.h"

#include "Frost/Platform/Vulkan/VulkanRenderer.h"

namespace Frost
{
	struct RendererData
	{
		Ref<ShaderLibrary> m_ShaderLibrary;
		Ref<Texture2D> m_WhiteTexture;
		Ref<Texture2D> m_BRDFLut;
		Ref<SceneEnvironment> m_Environment;
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
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPass.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBRDeffered.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreethamSky.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PathTracer.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EquirectangularToCubeMap.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentIrradiance.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentMipFilter.glsl");
		
		// Init the pools
		s_RendererAPI->Init();

		// Environment cubemaps
		s_Data->m_Environment = SceneEnvironment::Create();
		s_Data->m_Environment->Load("Resources/EnvironmentMaps/dikhololo_night_4k.hdr");

		// LUTS
		s_Data->m_WhiteTexture = Texture2D::Create("Resources/LUT/White.jpg");
		s_Data->m_BRDFLut = Texture2D::Create("Resources/LUT/BRDF_LUT.tga");

		// Init the renderpasses
		s_RendererAPI->InitRenderPasses();
	}

	void Renderer::ShutDown()
	{
		s_RendererAPI->ShutDown();
		delete s_CommandQueue;
		delete s_Data;
	}

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

	Ref<SceneEnvironment> Renderer::GetSceneEnvironmentMap()
	{
		return s_Data->m_Environment;
	}

	Ref<Texture2D> Renderer::GetWhiteLUT()
	{
		return s_Data->m_WhiteTexture;
	}

	Ref<Texture2D> Renderer::GetBRDFLut()
	{
		return s_Data->m_BRDFLut;
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