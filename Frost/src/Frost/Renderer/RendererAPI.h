#pragma once

#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/RenderCommandQueue.h"
#include "Frost/Renderer/Mesh.h"

#include "Frost/Renderer/Material.h"
#include "Frost/Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Frost
{
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, Vulkan = 1, OpenGL = 2
		};
	public:
		virtual ~RendererAPI() {}

		virtual void Init() = 0;
		virtual void InitRenderPasses() = 0; // TODO: This is temporary
		virtual void Render() = 0;
		virtual void ShutDown() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void BeginScene(const EditorCamera& camera) = 0;
		virtual void EndScene() = 0;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform) = 0;
		virtual void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform) = 0;

		virtual Ref<Image2D> GetFinalImage(uint32_t id) const = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
	private:
		static API s_API;
		friend class Renderer;
	};

	class RenderQueue
	{
	public:
		RenderQueue();
		~RenderQueue();

		void SetCamera(const EditorCamera& camera);

		void Add(Ref<Mesh> mesh, const glm::mat4& transform);

		// TODO: Need a way to handle materials around different sceneRenderPasses
		void Add(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform) {}

		void Reset();
		uint32_t GetQueueSize() const { return (uint32_t)m_Data.size(); }
	public:
		struct RenderData
		{
			Ref<Mesh> Mesh = nullptr;
			glm::mat4 Transform{};
		};

		Vector<RenderQueue::RenderData> m_Data;

		EditorCamera Camera;
		glm::mat4 CameraViewMatrix;
		glm::mat4 CameraProjectionMatrix;
		glm::vec3 CameraPosition;

		uint32_t ViewPortWidth;
		uint32_t ViewPortHeight;
	};
}