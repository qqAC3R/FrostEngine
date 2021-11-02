#pragma once

#include "Frost/Renderer/RendererAPI.h"
#include "Frost/Renderer/SceneEnvironment.h"
#define FRAMES_IN_FLIGHT 3

namespace Frost
{
	struct RendererConfig
	{
		uint32_t FramesInFlight = 3;
		
		uint32_t EnvironmentMapResolution = 1024;
		
		uint32_t IrradianceMapResolution = 32;
		uint32_t IrradianceMapSamples = 512;
	};

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
		static Ref<Image2D> GetFinalImage(uint32_t id) { return s_RendererAPI->GetFinalImage(id); }
		static Ref<Texture2D> GetWhiteLUT();
		static Ref<Texture2D> GetBRDFLut();

		static Ref<ShaderLibrary> GetShaderLibrary();
		static Ref<SceneEnvironment> GetSceneEnvironmentMap();
		static RendererConfig GetRendererConfig() { return RendererConfig(); }
		static RendererAPI::API GetAPI() { return s_RendererAPI->s_API; }

		static void ExecuteCommandBuffer();

		static RenderCommandQueue& GetRenderCommandQueue();
	private:
		static RendererAPI* s_RendererAPI;
		friend class Application;
	};
}