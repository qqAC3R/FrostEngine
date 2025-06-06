#pragma once

#include "Frost/Renderer/RendererAPI.h"
#include "Frost/Renderer/SceneEnvironment.h"
#include "Frost/Renderer/RendererSettings.h"
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

		struct Renderer2DSettings
		{
			uint64_t MaxQuads = static_cast<uint64_t>(std::pow(2, 16)); // 65536
			uint64_t MaxLines = static_cast<uint64_t>(std::pow(2, 16)); // 65536
		} Renderer2D;

		// Maximum amount of meshes for the indirect drawing buffer
		uint64_t MaxMeshCount_GeometryPass = static_cast<uint64_t>(std::pow(2, 14)); // 16834

		// Environment Maps
		uint32_t EnvironmentMapResolution = 1024;
		uint32_t IrradianceMapResolution = 32;
		uint32_t IrradianceMapSamples = 512;

		// Forward+
		uint32_t MaxPointLightCount = static_cast<uint32_t>(std::pow(2, 10)); // 1024
		uint32_t MaxRectangularLightCount = static_cast<uint32_t>(std::pow(2, 10)); // 64

		// Shadow Pass
		uint32_t ShadowTextureResolution = 2048;

		// Voxelization Pass
		uint32_t VoxelTextureResolution = 256;

		// Volumetric Pass
		uint32_t VoluemtricFroxelSlicesZ = 128;
	};

	// Memory Usage:
	/*
		Shadow Map = 67MB * 3 = 201MB
		3D Voxel texture = 67MB * 3 = 201MB
		   - Clear Buffer - 67MB * 1 = 67MB

		Volumetric Froxels - (41MB * 3 textures) * 3 FIF = 123MB
	
		GBuffer = (8MB * 4 attachments + 4MB (depth) ) * 3 = 108MB
	
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

		// Submit rendering commands to the graphics queue
		static void SubmitCmdsToRender() { s_RendererAPI->SubmitCmdsToRender(); }

		static void BeginScene(Ref<Scene> scene, Ref<EditorCamera>& camera) { s_RendererAPI->BeginScene(scene, camera); }
		static void BeginScene(Ref<Scene> scene, Ref<RuntimeCamera>& camera) { s_RendererAPI->BeginScene(scene, camera); }
		static void EndScene() { s_RendererAPI->EndScene(); }

		static void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityID = UINT32_MAX);
		static void Submit(const PointLightComponent& pointLight, const glm::vec3& position);
		static void Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction);
		static void Submit(const RectangularLightComponent& rectLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);
		static void Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform);
		static void Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale);
		static void SetSky(const SkyLightComponent& skyLightComponent);
		static void SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color);
		static void SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture);
		static void SubmitLines(const glm::vec3& positon0, const glm::vec3& positon1, const glm::vec4& color);
		static void SubmitText(const std::string& string, const Ref<Font>& font, const glm::mat4& transform, float maxWidth, float lineHeightOffset = 0.0f, float kerningOffset = 0.0f, const glm::vec4& color = glm::vec4(1.0f));
		static void SubmitWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f), float lineWidth = 1.0f);

		static uint32_t ReadPixelFromFramebufferEntityID(uint32_t x, uint32_t y);
		static uint32_t GetCurrentFrameIndex();
		static uint64_t GetFrameCount();
		static Ref<Scene> GetActiveScene();

		// We must request a function returning a texture, instead of texture,
		// because the renderer has 3 frames in flight
		static void SubmitImageToOutputImageMap(const std::string& name, const std::function<Ref<Image2D>()>& func);

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

		template<typename FuncT>
		static void SubmitDeletion(FuncT&& func)
		{
			auto renderCmd = [](void* ptr) {
				auto pFunc = (FuncT*)ptr;
				(*pFunc)();

				pFunc->~FuncT();
			};
			auto storageBuffer = GetDeletionCommandQueue().Allocate(renderCmd, sizeof(func));
			new (storageBuffer) FuncT(std::forward<FuncT>(func));
		}

		static void Resize(uint32_t width, uint32_t height) { s_RendererAPI->Resize(width, height); }

		static void RenderDebugger();

		static Ref<Texture2D> GetWhiteLUT();
		static Ref<Texture2D> GetBRDFLut();
		static Ref<Texture2D> GetNoiseLut();
		static Ref<Texture2D> GetSpatialBlueNoiseLut();
		static Ref<Texture2D> GetTemporalNoiseLut();
		static Ref<Texture2D> GetInternalEditorIcon(const std::string& name);

		static Ref<ShaderLibrary> GetShaderLibrary();
		static Ref<SceneEnvironment> GetSceneEnvironment();
		static const RendererConfig& GetRendererConfig();
		static RendererSettings& GetRendererSettings() { return s_RendererSettings; }
		static RendererAPI::API GetAPI() { return s_RendererAPI->s_API; }

		static Ref<Image2D> GetFinalImage(uint32_t id) { return s_RendererAPI->GetFinalImage(id); } // TODO: Remove
		static Ref<Image2D> GetFinalImage(const std::string& name);
		static const HashMap<std::string, std::function<Ref<Image2D>()>>& GetOutputImageMap();

		static Ref<Font> GetDefaultFont();

		static void ExecuteCommandBuffer();
		static void ExecuteDeletionCommands();

		static RenderCommandQueue& GetRenderCommandQueue();
	private:
		static RenderCommandQueue& GetDeletionCommandQueue();
		static void InitEditorIcons();
	private:
		static RendererAPI* s_RendererAPI;
		static RendererSettings s_RendererSettings;
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