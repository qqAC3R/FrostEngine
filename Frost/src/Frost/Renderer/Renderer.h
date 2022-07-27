#pragma once

#include "Frost/Renderer/RendererAPI.h"
#include "Frost/Renderer/SceneEnvironment.h"
#define FRAMES_IN_FLIGHT 3

namespace Frost
{
	// Renderer Specs:
	//     Indirect drawing:
	//		   Maximum instances:        1,024
	//         Maximum submeshes:		10,240
	//     Ray Tracing:
	//		   Maximum instances:        1,024
	//         Maximum submeshes:		10,240
	//         Maximum submeshes Count:  1,024

	struct RendererConfig
	{
		uint32_t FramesInFlight = FRAMES_IN_FLIGHT;
		

		struct RayTracingSettings
		{
			// TODO: The rasterizer and the rt should have the same number of instances support (currently the rt has 2k, rasterizer has 16k)
			uint32_t MaxMesh = static_cast<uint32_t>(std::pow(2, 12)); // 4096
			uint32_t MaxInstance = static_cast<uint32_t>(std::pow(2, 10)); // 1024
		} RayTracing;

		// Maximum amount of meshes for the indirect drawing buffer
		uint64_t MaxMeshCount_GeometryPass = static_cast<uint64_t>(std::pow(2, 14)); // 16834

		// Environment Maps
		uint32_t EnvironmentMapResolution = 1024;
		uint32_t IrradianceMapResolution = 32;
		uint32_t IrradianceMapSamples = 512;

		// Forward+
		uint32_t MaxPointLightCount = static_cast<uint32_t>(std::pow(2, 10)); // 1024

		// Shadow Pass
		uint32_t ShadowTextureResolution = 2048;

		// Voxelization Pass
		uint32_t VoxelTextureResolution = 256;

		// Volumetric Pass
		uint32_t VoluemtricFroxelSlicesZ = 128;
	};

	// Memory Usage:
	/*
		Shadow Map = 64mb * 3 = 192mb
		3D Voxel texture = 73.15mb * 3 = 219.45mb
	
		GBuffer = (8mb * 4 attachments + 4mb (depth) ) * 3 = 108mb
	
		Radiance Map = 64mb
	
		All RGBA8 Images (from compute shaders) = 5mb * 7 * 3 = 105mb
		2 Bloom textures 16F = 10mb * 2 * 3 = 60mb
	*/

	// Forward declaration
	struct PointLightComponent;

	class Renderer
	{
	public:
		static void Init();
		static void ShutDown();

		static void BeginFrame() { s_RendererAPI->BeginFrame(); }
		static void EndFrame() { s_RendererAPI->EndFrame(); }

		static void BeginScene(const EditorCamera& camera) { s_RendererAPI->BeginScene(camera); }
		static void EndScene() { s_RendererAPI->EndScene(); }

		static void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform);
		static void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform);
		static void Submit(const PointLightComponent& pointLight, const glm::vec3& position);
		static void Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction);
		static void Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform);
		static void Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale);

		template<typename FuncT>
		static void Submit(FuncT&& func)
		{
			auto renderCmd = [](void* ptr) {
				auto pFunc = (FuncT*)ptr;
				(*pFunc)();

				pFunc->~FuncT();
			};
			auto storageBuffer = GetRenderCommandQueue().Allocate(renderCmd, sizeof(func));
			new (storageBuffer) FuncT(std::forward<FuncT>(func));
		}

		static void Resize(uint32_t width, uint32_t height) { s_RendererAPI->Resize(width, height); }
		static void LoadEnvironmentMap(const std::string& filepath);

		static void RenderDebugger();

		static Ref<Image2D> GetFinalImage(uint32_t id) { return s_RendererAPI->GetFinalImage(id); }
		static Ref<Texture2D> GetWhiteLUT();
		static Ref<Texture2D> GetBRDFLut();
		static Ref<Texture2D> GetNoiseLut();
		static Ref<Texture2D> GetSpatialBlueNoiseLut();
		static Ref<Texture2D> GetTemporalNoiseLut();

		static Ref<ShaderLibrary> GetShaderLibrary();
		static Ref<SceneEnvironment> GetSceneEnvironment();
		static const RendererConfig& GetRendererConfig();
		static RendererAPI::API GetAPI() { return s_RendererAPI->s_API; }

		static void ExecuteCommandBuffer();

		static RenderCommandQueue& GetRenderCommandQueue();
	private:
		static RendererAPI* s_RendererAPI;
		friend class Application;
	};

	// Forward declaration
	class SceneRenderPassPipeline;

	class RendererDebugger
	{
	public:
		virtual ~RendererDebugger() {};
		
		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) = 0;
		virtual void ImGuiRender() = 0;

		virtual void StartTimeStapForPass(const std::string& passName) = 0;
		virtual void EndTimeStapForPass(const std::string& passName) = 0;

		static Ref<RendererDebugger> Create();
	};
}