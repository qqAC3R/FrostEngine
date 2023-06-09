#pragma once

#include "Frost/Renderer/RenderPass.h"
#include "Frost/Renderer/RenderCommandQueue.h"
#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/Material.h"
#include "Frost/Renderer/EditorCamera.h"

#include "Frost/EntitySystem/Components.h"

#include <glm/glm.hpp>

namespace Frost
{
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, Vulkan = 1
		};
	public:
		virtual ~RendererAPI() {}

		virtual void Init() = 0;
		virtual void InitRenderPasses() = 0;
		virtual void Render() = 0;
		virtual void RenderDebugger() = 0;
		virtual void ShutDown() = 0;

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void SubmitCmdsToRender() = 0;

		virtual void BeginScene(Ref<EditorCamera> camera) = 0;
		virtual void BeginScene(Ref<RuntimeCamera> camera) = 0;
		virtual void EndScene() = 0;

		virtual void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform, uint32_t entityID) = 0;
		virtual void Submit(const PointLightComponent& pointLight, const glm::vec3& position) = 0;
		virtual void Submit(const DirectionalLightComponent& directionalLight, const glm::vec3& direction) = 0;
		virtual void Submit(const RectangularLightComponent& rectLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale) = 0;
		virtual void Submit(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform) = 0;
		virtual void Submit(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale) = 0;
		virtual void SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color) = 0;
		virtual void SubmitBillboards(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture) = 0;
		virtual void SubmitWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color, float lineWidth) = 0;

		virtual uint32_t ReadPixelFromFramebufferEntityID(uint32_t x, uint32_t y) = 0;
		virtual uint32_t GetCurrentFrameIndex() = 0;
		virtual void SetEditorActiveEntity(uint32_t selectedEntityId) = 0;

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

		void SetCamera(Ref<EditorCamera> camera);
		void SetCamera(Ref<RuntimeCamera> camera);
		void SetEditorActiveEntity(uint32_t selectedEntityId) { m_SelectedEntityID = selectedEntityId; }

		void Add(Ref<Mesh> mesh, const glm::mat4& transform, uint32_t entityID = UINT32_MAX);
		void AddWireframeMesh(Ref<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f), float lineWidth = 1.0f);
		void AddFogVolume(const FogBoxVolumeComponent& fogVolume, const glm::mat4& transform);
		void AddCloudVolume(const CloudVolumeComponent& cloudVolume, const glm::vec3& position, const glm::vec3& scale);
		void AddBillBoard(const glm::vec3& positon, const glm::vec2& size, const glm::vec4& color);
		void AddBillBoard(const glm::vec3& positon, const glm::vec2& size, Ref<Texture2D> texture);
		void AddPointLight(const PointLightComponent& pointLight, const glm::vec3& position);
		void AddRectangularLight(const RectangularLightComponent& rectangularLight, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);
		void SetDirectionalLight(const DirectionalLightComponent& directionalLight, const glm::vec3& direction);

		void Reset();
		uint32_t GetQueueSize() const { return static_cast<uint32_t>(m_Data.size()); }
		uint32_t GetSceneSubmeshCount() const { return m_SubmeshCount; }
	public:
		struct RenderData
		{
			Ref<Mesh> Mesh = nullptr;
			glm::mat4 Transform{};
			uint32_t EntityID;
		};
		Vector<RenderQueue::RenderData> m_Data;
		uint32_t m_SelectedEntityID;

		struct LightData
		{
			// Packed them into a struct because the memory should be contiguous (for better performance)
			struct PointLight
			{
				PointLightComponent Specification;
				glm::vec3 Position;
			};

			struct DirectionalLight
			{
				DirectionalLightComponent Specification;
				glm::vec3 Direction;
			};

			struct RectangularLightData
			{
				glm::vec3 Radiance;
				float Intensity;
				glm::vec4 Vertex0; // W component stores the TwoSided bool
				glm::vec4 Vertex1; // W component stores the Radius for it to be culled
				glm::vec4 Vertex2;
				glm::vec4 Vertex3;
			};

			Vector<PointLight> PointLights;
			Vector<RectangularLightData> RectangularLights;
			DirectionalLight DirLight;
		};
		LightData m_LightData;

		// 2D Batched objects
		struct Object2D
		{
			enum class ObjectType
			{
				Quad, Billboard
			};
			ObjectType Type = ObjectType::Quad;

			glm::vec3 Position{ 0.0f };
			glm::vec2 Size{ 1.0f };
			glm::vec4 Color{ 1.0f };
			//uint32_t TexIndex = 0;
			Ref<Texture2D> Texture;
		};
		Vector<Object2D> m_BatchRendererData;

		struct RenderWireframeData
		{
			Ref<MeshAsset> Mesh = nullptr;
			glm::mat4 Transform{};
			glm::vec4 Color;
			float LineWidth = 1.0f;
		};
		Vector<RenderWireframeData> m_WireframeRenderData;
		
		// Fog Volume Data
		struct FogVolume
		{
			glm::mat4 InverseTransformMat; // Due to buffer padding reasons, the mat4 should be first
			glm::vec3 MieScattering = { 1.0f, 1.0f, 1.0f };
			float PhaseValue = 0.0f;
			glm::vec3 Emission = { 0.0f, 0.0f, 0.0f };
			float Absorption = 1.0f;
		};
		Vector<FogVolume> m_FogVolumeData;

		// Cloud Volume Data
		struct CloudVolume
		{
			glm::vec3 Position;
			float CloudScale;

			glm::vec3 Scale;
			float Density;

			glm::vec3 Scattering;
			float PhaseFunction;

			float DensityOffset;
			float CloudAbsorption;
			float SunAbsorption;
			float DetailOffset;
		};
		Vector<CloudVolume> m_CloudVolumeData;


		uint32_t m_SubmeshCount = 0;

		enum class CameraType
		{
			Editor, Runtime
		};
		
		Ref<Camera> m_Camera;
		glm::mat4 CameraViewMatrix;
		glm::mat4 CameraProjectionMatrix;
		glm::vec3 CameraPosition;

		uint32_t ViewPortWidth;
		uint32_t ViewPortHeight;
	};
}