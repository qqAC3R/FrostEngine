#include "frostpch.h"
#include "RendererAPI.h"

namespace Frost
{
	RendererAPI::API RendererAPI::s_API = RendererAPI::API::Vulkan;


	RenderQueue::RenderQueue()
	{
		m_Data.reserve(1024);
	}
	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::SetCamera(const EditorCamera& camera)
	{
		Camera = camera;
		CameraViewMatrix = camera.GetViewMatrix();
		CameraProjectionMatrix = camera.GetProjectionMatrix();
		CameraPosition = camera.GetPosition();
		ViewPortWidth = (uint32_t)camera.m_ViewportWidth;
		ViewPortHeight = (uint32_t)camera.m_ViewportHeight;
	}

	void RenderQueue::Add(Ref<Mesh> mesh, const glm::mat4& transform)
	{
		RenderQueue::RenderData data{};
		data.Mesh = mesh;
		data.Transform = transform;

		m_Data.push_back(data);
		m_SubmeshCount += mesh->GetSubMeshes().size();
	}

	void RenderQueue::Reset()
	{
		m_Data.clear();
		CameraViewMatrix = glm::mat4(1.0f);
		CameraProjectionMatrix = glm::mat4(1.0f);
	}
}