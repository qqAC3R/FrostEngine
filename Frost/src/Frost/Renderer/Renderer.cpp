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
		
		HashMap<std::string, std::function<Ref<Image2D>()>> m_OutputImageMap;

		HashMap<std::string, Ref<Texture2D>> EditorIcons;
		DefaultMeshStorage DefaultMeshes;

		Ref<Font> DefaultFont;
	};

	RendererAPI* Renderer::s_RendererAPI = nullptr;

	RendererSettings Renderer::s_RendererSettings;

	static RenderCommandQueue* s_CommandQueue = nullptr;
	static RenderCommandQueue* s_DeletionCommandQueue = nullptr;
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
		s_DeletionCommandQueue = new RenderCommandQueue();
		s_Data = new RendererData();

		// Renderer settings initialization (in the future, this could be changed into importing values from files)
		s_RendererSettings.Initialize();

		// Init the shaders
		s_Data->m_ShaderLibrary = Ref<ShaderLibrary>::Create();

		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPassIndirectBindless.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPass.glsl");
		//Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBRDeffered.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/PBRDeffered_Compute.glsl");
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
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/TiledPointLightCulling.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/TiledRectangularLightCulling.glsl");
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
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/BatchRendererQuad.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/BatchRendererLine.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/Wireframe.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/SceneGrid.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/EntityGlow.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/LineDetection.glsl");
		Renderer::GetShaderLibrary()->Load("Resources/Shaders/GeometryPassIndirectInstancedBindless.glsl");
		//Renderer::GetShaderLibrary()->Load("Resources/Shaders/BloomConvolution.glsl");

		
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

		InitEditorIcons();
		MeshAsset::InitDefaultMeshes();
		s_Data->DefaultFont = Ref<Font>::Create("Resources/Fonts/san-francisco/SF-Regular.otf");

		// Environment cubemaps
		s_Data->m_Environment = SceneEnvironment::Create();

		// Init the renderpasses
		s_RendererAPI->InitRenderPasses();

		s_Data->m_Environment->InitCallbackFunctions();
	}

	void Renderer::ShutDown()
	{
		s_RendererAPI->ShutDown();
		MeshAsset::DestroyDefaultMeshes();
		delete s_CommandQueue;
		delete s_Data;
	}

	void Renderer::Submit(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityID)
	{
		s_RendererAPI->Submit(mesh, transform, entityID);
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

	void Renderer::Submit(const RectangularLightComponent& rectLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
	{
		s_RendererAPI->Submit(rectLight, position, rotation, scale);
	}

	void Renderer::SetSky(const SkyLightComponent& skyLightComponent)
	{
		if (s_Data->m_Environment->GetRadianceMap().Raw() != skyLightComponent.RadianceMap.Raw())
		{
			s_Data->m_Environment->SetHDREnvironmentMap(skyLightComponent.RadianceMap, skyLightComponent.PrefilteredMap, skyLightComponent.IrradianceMap);
		}
	}

	void Renderer::SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color)
	{
		s_RendererAPI->SubmitBillboards(positon, size, color);
	}

	void Renderer::SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture)
	{
		s_RendererAPI->SubmitBillboards(positon, size, texture);
	}

	void Renderer::SubmitLines(const glm::vec3& positon0, const glm::vec3& positon1, const glm::vec4& color)
	{
		s_RendererAPI->SubmitLines(positon0, positon1, color);
	}

	void Renderer::SubmitText(const std::string& string, const Ref<Font>& font, const glm::mat4& transform, float maxWidth, float lineHeightOffset, float kerningOffset, const glm::vec4& color)
	{
		s_RendererAPI->SubmitText(string, font, transform, maxWidth, lineHeightOffset, kerningOffset, color);
	}

	void Renderer::SubmitWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color, float lineWidth)
	{
		s_RendererAPI->SubmitWireframeMesh(mesh, transform, color, lineWidth);
	}

	uint32_t Renderer::ReadPixelFromFramebufferEntityID(uint32_t x, uint32_t y)
	{
		return s_RendererAPI->ReadPixelFromFramebufferEntityID(x, y);
	}

	uint32_t Renderer::GetCurrentFrameIndex()
	{
		return s_RendererAPI->GetCurrentFrameIndex();
	}

	void Renderer::SetEditorActiveEntity(uint32_t selectedEntityId)
	{
		s_RendererAPI->SetEditorActiveEntity(selectedEntityId);
	}

	void Renderer::SubmitImageToOutputImageMap(const std::string& name, const std::function<Ref<Image2D>()>& func)
	{
		s_Data->m_OutputImageMap[name] = func;
	}

	void Renderer::RenderDebugger()
	{
		s_RendererAPI->RenderDebugger();
	}

	Ref<Image2D> Renderer::GetFinalImage(const std::string& name)
	{
		return s_Data->m_OutputImageMap[name]();
	}

	const HashMap<std::string, std::function<Ref<Image2D>()>>& Renderer::GetOutputImageMap()
	{
		return s_Data->m_OutputImageMap;
	}

	Ref<Font> Renderer::GetDefaultFont()
	{
		return s_Data->DefaultFont;
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

	Ref<Texture2D> Renderer::GetInternalEditorIcon(const std::string& name)
	{
		return s_Data->EditorIcons[name];
	}

	void Renderer::ExecuteCommandBuffer()
	{
		s_CommandQueue->Execute();
	}

	void Renderer::ExecuteDeletionCommands()
	{
		s_DeletionCommandQueue->Execute();
	}

	RenderCommandQueue& Renderer::GetRenderCommandQueue()
	{
		return *s_CommandQueue;
	}

	RenderCommandQueue& Renderer::GetDeletionCommandQueue()
	{
		return *s_DeletionCommandQueue;
	}

	void Renderer::InitEditorIcons()
	{
		TextureSpecification textureSpec{};
		textureSpec.Usage = ImageUsage::ReadOnly;
		textureSpec.Format = ImageFormat::RGBA16F;
		textureSpec.UseMips = false;

		s_Data->EditorIcons["PointLight"] = Texture2D::Create("Resources/Editor/PointLight.png", textureSpec);
		s_Data->EditorIcons["DirectionalLight"] = Texture2D::Create("Resources/Editor/DirectionalLight.png", textureSpec);
		s_Data->EditorIcons["RectangularLight"] = Texture2D::Create("Resources/Editor/RectangularLight.png", textureSpec);
		s_Data->EditorIcons["SceneCamera"] = Texture2D::Create("Resources/Editor/SceneCamera.png", textureSpec);
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