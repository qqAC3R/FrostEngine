#include "frostpch.h"
#include "Renderer.h"

#include "Frost/Platform/Vulkan/VulkanRenderer.h"
#include "Frost/Platform/Vulkan/VulkanRendererDebugger.h"
#include "Frost/Utils/Timer.h"

namespace Frost
{
	struct RendererData
	{
		Ref<ShaderLibrary> m_ShaderLibrary;
		Ref<Texture2D> m_WhiteTexture;
		Ref<Texture2D> m_BRDFLut;
		Ref<Texture2D> m_NoiseLut;
		Ref<Texture2D> m_SpatialBlueNoiseLut;
		Ref<Texture2D> m_TemporalBlueNoiseLut;
		Ref<SceneEnvironment> m_Environment;
		//Ref<RendererDebugger> m_RendererDebugger;
	};

	RendererAPI* Renderer::s_RendererAPI = nullptr;
	static RenderCommandQueue* s_CommandQueue = nullptr;
	static RendererData* s_Data = nullptr;
	static RendererConfig s_RendererConfig = {};

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

		//Vector<ShaderArray> shaderArray;
		//shaderArray.emplace_back("u_AlbedoTexture", 1024);

		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPassIndirectBindless.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPass.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBRDeffered.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PreethamSky.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PathTracer.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EquirectangularToCubeMap.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentIrradiance.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EnvironmentMipFilter.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/RenderSkybox.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPassIndirect.glsl");
		//Renderer::GetShaderLibrary()->Load("Resources/Shaders/OcclusionCulling.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/OcclusionCulling_V2.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/HiZBufferBuilder.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/TiledLightCulling.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/ScreenSpaceReflections.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GaussianBlur.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VisibilityBuffer.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/AmbientOcclusion.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SpatialDenoiser.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Bloom.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/ColorCorrection.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Transmittance.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/MultiScatter.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SkyViewBuilder.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SkyViewIrradiance.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SkyViewFilter.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/AerialPerspective.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/ApplyAerial.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Voxelization.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VoxelFilter.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/ShadowDepthPass.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/ShadowCompute.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VoxelConeTracing.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/FroxelVolumePopulate.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VolumetricCompute.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VolumetricBlur.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VolumetricInjectLight.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VolumetricGatherLight.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/VolumetricTAA.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/CloudPerlinNoise.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/CloudWoorleyNoise.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/CloudComputeVolumetric.glsl");

		
		// Init the pools
		s_RendererAPI->Init();

		// LUTS
		TextureSpecification textureSpec{};
		textureSpec.Usage = ImageUsage::ReadOnly;
		textureSpec.UseMips = false;

		// White texture
		s_Data->m_WhiteTexture = Texture2D::Create("Resources/LUT/White.jpg", textureSpec);

		// BRDF LUT
		textureSpec.Format = ImageFormat::RGBA16F;
		s_Data->m_BRDFLut = Texture2D::Create("Resources/LUT/BRDF_LUT.tga", textureSpec);

		// Blue noise
		s_Data->m_SpatialBlueNoiseLut = Texture2D::Create("Resources/LUT/BlueNoise.png", textureSpec);
		s_Data->m_TemporalBlueNoiseLut = Texture2D::Create("Resources/LUT/TemporalNoise.png", textureSpec);

		// Simple 4x4 noise
		textureSpec.Sampler.SamplerFilter = ImageFilter::Nearest;
		textureSpec.Format = ImageFormat::RG32F;
		s_Data->m_NoiseLut = Texture2D::Create("Resources/LUT/Noise.png", textureSpec);

		// Environment cubemaps
		s_Data->m_Environment = SceneEnvironment::Create();

		// Init the renderpasses
		s_RendererAPI->InitRenderPasses();

		// Create the renderer debugger
		//s_Data->m_RendererDebugger = RendererDebugger::Create();
		//s_Data->m_RendererDebugger->Init();

		s_Data->m_Environment->InitCallbackFunctions();
		//s_Data->m_Environment->LoadEnvMap("Resources/EnvironmentMaps/pink_sunrise_4k.hdr");
		//s_Data->m_Environment->LoadEnvMap("Resources/EnvironmentMaps/dikhololo_night_4k.hdr");
		//s_Data->m_Environment->LoadEnvMap("Resources/EnvironmentMaps/envmap.JPG");
		//s_Data->m_Environment->SetType(SceneEnvironment::Type::Hillaire);
		//s_Data->m_Environment->SetType(SceneEnvironment::Type::HDRMap);
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

	void Renderer::Submit(const PointLightComponent& pointLight, const glm::vec3& position)
	{
		s_RendererAPI->Submit(pointLight, position);
	}

	void Renderer::Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction)
	{
		s_RendererAPI->Submit(directionalLight, direction);
	}

	void Renderer::Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform)
	{
		s_RendererAPI->Submit(fogVolume, transform);
	}

	void Renderer::Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale)
	{
		s_RendererAPI->Submit(cloudVolume, position, scale);
	}

	void Renderer::LoadEnvironmentMap(const std::string& filepath)
	{
		// TODO: Add some function for the SceneRenderpasses to update the env texture with the new ones
		Renderer::Submit([&, filepath]() mutable
		{
			s_Data->m_Environment->LoadEnvMap(filepath);
		});
	}

	void Renderer::RenderDebugger()
	{
		s_RendererAPI->RenderDebugger();
	}

	Ref<ShaderLibrary> Renderer::GetShaderLibrary()
	{
		return s_Data->m_ShaderLibrary;
	}

	Ref<SceneEnvironment> Renderer::GetSceneEnvironment()
	{
		return s_Data->m_Environment;
	}

	const RendererConfig& Renderer::GetRendererConfig()
	{
		return s_RendererConfig;
	}

	Ref<Texture2D> Renderer::GetWhiteLUT()
	{
		return s_Data->m_WhiteTexture;
	}

	Ref<Texture2D> Renderer::GetBRDFLut()
	{
		return s_Data->m_BRDFLut;
	}

	Ref<Texture2D> Renderer::GetNoiseLut()
	{
		return s_Data->m_NoiseLut;
	}

	Ref<Texture2D> Renderer::GetSpatialBlueNoiseLut()
	{
		return s_Data->m_SpatialBlueNoiseLut;
	}

	Ref<Texture2D> Renderer::GetTemporalNoiseLut()
	{
		return s_Data->m_TemporalBlueNoiseLut;
	}

	void Renderer::ExecuteCommandBuffer()
	{
		s_CommandQueue->Execute();
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return *s_CommandQueue;
	}

	Ref<RendererDebugger> RendererDebugger::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   FROST_ASSERT(false, "Renderer::API::None is not supported!");
			case RendererAPI::API::Vulkan: return CreateRef<VulkanRendererDebugger>();
		}

		FROST_ASSERT_MSG("Unknown RendererAPI!");
		return nullptr;
	}

}