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

		Vector<ShaderArray> shaderArray;
		shaderArray.emplace_back("u_AlbedoTexture", 1024);

		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPassIndirectBindless.glsl", shaderArray);
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPass.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBRDeffered.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreethamSky.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PathTracer.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EquirectangularToCubeMap.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentIrradiance.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentMipFilter.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/RenderSkybox.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPassIndirect.glsl");
		
		// Init the pools
		s_RendererAPI->Init();

		// Environment cubemaps
		s_Data->m_Environment = SceneEnvironment::Create();
		s_Data->m_Environment->Load("Resources/EnvironmentMaps/pink_sunrise_4k.hdr");

		// LUTS
		TextureSpecification textureSpec{};
		textureSpec.UseMips = false;
		s_Data->m_WhiteTexture = Texture2D::Create("Resources/LUT/White.jpg", textureSpec);
		s_Data->m_BRDFLut = Texture2D::Create("Resources/LUT/BRDF_LUT.tga", textureSpec);

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

	void Renderer::LoadEnvironmentMap(const std::string& filepath)
	{
		// TODO: Add some function for the SceneRenderpasses to update the env texture with the new ones
		Renderer::Submit([&, filepath]() mutable
		{
			s_Data->m_Environment->Load(filepath);
		});
	}

	Ref<ShaderLibrary> Renderer::GetShaderLibrary()
	{
		return s_Data->m_ShaderLibrary;
	}

	Ref<SceneEnvironment> Renderer::GetSceneEnvironment()
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