#pragma once

#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Material.h"
#include "Frost/Renderer/EditorCamera.h"

#include "Frost/Renderer/RenderPass.h"

#include <glm/glm.hpp>

namespace Frost
{
	class RenderSubmitQueue
	{
	public:
		RenderSubmitQueue() = default;
		virtual ~RenderSubmitQueue() {}

		void Submit(std::function<void()> func)
		{
			m_Queue.push(func);
		}

		void Run()
		{
			while (!m_Queue.empty())
			{
				auto func = m_Queue.front();
				func();
				m_Queue.pop();
			}
		}

	private:
		std::queue<std::function<void()>> m_Queue;
	};

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
		virtual void ShutDown() = 0;

		virtual void BeginScene(const EditorCamera& camera) = 0;
		virtual void EndScene() = 0;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform) = 0;
		virtual void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform) = 0;


		inline void Submit(std::function<void()> func) { s_RenderSubmitQueue.Submit(func); }
		inline static API GetAPI() { return s_API; }
	protected:
		virtual void Update() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		RenderSubmitQueue s_RenderSubmitQueue;
	private:
		static API s_API;
		friend class Renderer;
	};

	class RenderQueue
	{
	public:
		RenderQueue() = default;

		void SetCamera(const EditorCamera& camera)
		{
			CameraViewMatrix = camera.GetViewMatrix();
			CameraProjectionMatrix = camera.GetProjectionMatrix();
		}

		void Add(const Ref<Mesh>& mesh, const glm::mat4& transform)
		{
			auto& data = m_Data.emplace_back();
			data.Mesh = mesh;
			data.Material = nullptr;
			data.Transform = transform;
		}

		// TODO: Need a way to handle materials around different sceneRenderPasses
		void Add(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform) {}

		void Reset() {
			m_Data.clear();
			CameraViewMatrix = glm::mat4(1.0f);
			CameraProjectionMatrix = glm::mat4(1.0f);
		}

	public:
		struct Data
		{
			Ref<Mesh> Mesh;
			glm::mat4 Transform;
			Ref<Material> Material;
		};

		// TODO: Should be a queue tho :P
		Vector<Data> m_Data;

		glm::mat4 CameraViewMatrix;
		glm::mat4 CameraProjectionMatrix;
	};


}