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
		m_CloudVolumeData.reserve(1024);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::SetCamera(Ref<EditorCamera> camera)
	{
		m_Camera = camera.As<Camera>();
		CameraViewMatrix = camera->GetViewMatrix();
		CameraProjectionMatrix = camera->GetProjectionMatrix();
		CameraPosition = camera->GetPosition();
		ViewPortWidth = (uint32_t)camera->m_ViewportWidth;
		ViewPortHeight = (uint32_t)camera->m_ViewportHeight;
	}

	void RenderQueue::SetCamera(Ref<RuntimeCamera> camera)
	{
		m_Camera = camera.As<Camera>();
		CameraViewMatrix = camera->GetViewMatrix();
		CameraProjectionMatrix = camera->GetProjectionMatrix();
		CameraPosition = camera->GetPosition();
		ViewPortWidth = (uint32_t)camera->m_ViewportWidth;
		ViewPortHeight = (uint32_t)camera->m_ViewportHeight;
	}

	void RenderQueue::Add(Ref<Mesh> mesh, const glm::mat4& transform, uint32_t entityID)
	{
		RenderQueue::RenderData& data = m_Data.emplace_back();
		data.Mesh = mesh;
		data.Transform = transform;
		data.EntityID = entityID;

		m_SubmeshCount += mesh->GetMeshAsset()->GetSubMeshes().size();
	}

	void RenderQueue::AddWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color, float lineWidth)
	{
		m_WireframeRenderData.push_back({
			mesh,
			transform,
			color,
			lineWidth
		});
	}

	void RenderQueue::AddPointLight(const PointLightComponent& pointLight, const glm::vec3& position)
	{
		m_LightData.PointLights.push_back({ pointLight, position });
	}

	void RenderQueue::AddRectangularLight(const RectangularLightComponent& rectangularLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)*
							  glm::toMat4(glm::quat(rotation)) *
							  glm::scale(glm::mat4(1.0f), scale);

		glm::vec3 v0 = glm::vec3(transform * glm::vec4(-0.5f, -0.5f, 0.0f, 1.0f));
		glm::vec3 v1 = glm::vec3(transform * glm::vec4(-0.5f,  0.5f, 0.0f, 1.0f));
		glm::vec3 v2 = glm::vec3(transform * glm::vec4( 0.5f,  0.5f, 0.0f, 1.0f));
		glm::vec3 v3 = glm::vec3(transform * glm::vec4( 0.5f, -0.5f, 0.0f, 1.0f));

		m_LightData.RectangularLights.push_back({
			rectangularLight.Radiance,
			rectangularLight.Intensity,
			glm::vec4(v0, float(rectangularLight.TwoSided)),
			glm::vec4(v1, rectangularLight.Radius), // Radius
			glm::vec4(v2, 1.0f),
			glm::vec4(v3, 1.0f),
		});
	}

	void RenderQueue::AddFogVolume(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform)
	{
		m_FogVolumeData.push_back({
			glm::inverse(transform),
			fogVolume.MieScattering * fogVolume.Density,
			fogVolume.PhaseValue,
			fogVolume.Emission * fogVolume.Density,
			fogVolume.Absorption * fogVolume.Density,
		});
	}

	void RenderQueue::AddCloudVolume(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale)
	{
		m_CloudVolumeData.push_back({
			position,
			cloudVolume.CloudScale,
			scale,
			cloudVolume.Density,
			cloudVolume.Scattering,
			cloudVolume.PhaseFunction,
			cloudVolume.DensityOffset,
			cloudVolume.CloudAbsorption,
			cloudVolume.SunAbsorption,
			cloudVolume.DetailOffset
		});
	}

	void RenderQueue::AddBillBoard(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color)
	{
		m_BatchRendererData.push_back({
			Object2D::ObjectType::Billboard,
			positon,
			size,
			color,
			nullptr
		});
	}

	void RenderQueue::AddBillBoard(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture)
	{
		m_BatchRendererData.push_back({
			Object2D::ObjectType::Billboard,
			positon,
			size,
			glm::vec4(1.0f),
			texture
		});
	}

	void RenderQueue::SetDirectionalLight(const DirectionalLightComponent& directionalLight, const glm::vec3& direction)
	{
		m_LightData.DirLight.Specification = directionalLight;
		m_LightData.DirLight.Direction = direction;
	}

	void RenderQueue::Reset()
	{
		m_SubmeshCount = 0;
		m_Data.clear();
		m_LightData.PointLights.clear();
		m_LightData.RectangularLights.clear();
		m_FogVolumeData.clear();
		m_CloudVolumeData.clear();
		m_BatchRendererData.clear();
		m_WireframeRenderData.clear();
		CameraViewMatrix = glm::mat4(1.0f);
		CameraProjectionMatrix = glm::mat4(1.0f);
	}
}