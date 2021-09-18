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
		RenderQueue()
		{
			//m_Data.reserve(1024);
		}

		void SetCamera(const EditorCamera& camera)
		{
			CameraViewMatrix = camera.GetViewMatrix();
			CameraProjectionMatrix = camera.GetProjectionMatrix();
			CameraPosition = camera.GetPosition();
			ViewPortWidth = (uint32_t)camera.m_ViewportWidth;
			ViewPortHeight = (uint32_t)camera.m_ViewportHeight;
		}

		void Add(Ref<Mesh> mesh, const glm::mat4& transform)
		{
			RenderQueue::RenderData data{};
			//data.Mesh = mesh;
			//data.Material = nullptr;
			//data.Transform = transform;

			data.Mesh = mesh;
			data.Transform = transform;
			//data.Material = nullptr;
			//m_Data.push_back(data);

			m_Data[m_DataSize] = data;
			m_DataSize++;
		}

		// TODO: Need a way to handle materials around different sceneRenderPasses
		void Add(const Ref<Mesh>& mesh, const Ref<Material>& material, const glm::mat4& transform) {}

		void Reset()
		{
			//m_Data.clear();
			m_DataSize = 0;
			CameraViewMatrix = glm::mat4(1.0f);
			CameraProjectionMatrix = glm::mat4(1.0f);
		}

	public:
		//struct Data
		//{
		//	Ref<Mesh> Mesh;
		//	glm::mat4 Transform;
		//	Ref<Material> Material;
		//};

		struct RenderData
		{
			Ref<Mesh> Mesh = nullptr;
			glm::mat4 Transform{};
		};

		//Vector<RenderQueue::RenderData> m_Data;
		std::array<RenderQueue::RenderData, 128> m_Data;
		uint32_t m_DataSize = 0;

		glm::mat4 CameraViewMatrix;
		glm::mat4 CameraProjectionMatrix;
		glm::vec3 CameraPosition;

		uint32_t ViewPortWidth;
		uint32_t ViewPortHeight;
	};


}