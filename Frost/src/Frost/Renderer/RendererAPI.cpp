#include "frostpch.h"
#include "RendererAPI.h"

namespace Frost
{
	RendererAPI::API RendererAPI::s_API = RendererAPI::API::Vulkan;

	RenderQueue::RenderQueue()
	{
		m_Data.reserve(2048);
		m_LightData.PointLights.reserve(1024);
		m_FogVolumeData.reserve(1024);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::SetCamera(const EditorCamera& camera)
	{
		m_Camera = camera;
		CameraViewMatrix = camera.GetViewMatrix();
		CameraProjectionMatrix = camera.GetProjectionMatrix();
		CameraPosition = camera.GetPosition();
		ViewPortWidth = (uint32_t)camera.m_ViewportWidth;
		ViewPortHeight = (uint32_t)camera.m_ViewportHeight;
	}

	void RenderQueue::Add(Ref<Mesh> mesh, const glm::mat4& transform)
	{
		RenderQueue::RenderData& data = m_Data.emplace_back();
		data.Mesh = mesh;
		data.Transform = transform;

		m_SubmeshCount += mesh->GetSubMeshes().size();
	}

	void RenderQueue::AddPointLight(const PointLightComponent& pointLight, const glm::vec3& position)
	{
		m_LightData.PointLights.push_back({ pointLight, position });
	}

	void RenderQueue::AddFogVolume(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform)
	{
		m_FogVolumeData.push_back({ glm::inverse(transform), fogVolume });
	}

	void RenderQueue::SetDirectionalLight(const DirectionalLightComponent& directionalLight, const glm::vec3& direction)
	{
		m_LightData.DirectionalLight = directionalLight;
		m_LightData.DirectionalLight.Direction = direction;
	}

	void RenderQueue::Reset()
	{
		m_SubmeshCount = 0;
		m_Data.clear();
		m_LightData.PointLights.clear();
		m_FogVolumeData.clear();
		CameraViewMatrix = glm::mat4(1.0f);
		CameraProjectionMatrix = glm::mat4(1.0f);
	}
}