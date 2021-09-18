#pragma once

#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "Frost/Renderer/Texture.h"

#include "Frost/Math/BoundingBox.h"
#include <glm/glm.hpp>

#include "Frost/Renderer/RayTracing/AccelerationStructures.h"

namespace Frost
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec2 TexCoords;
		glm::vec3 Normal;
		glm::vec3 Tagent;
		glm::vec3 Bitangent;
	};

	struct Index
	{
		uint32_t V1, V2, V3;
	};

	struct MaterialInstance
	{
		glm::vec3 ambient = glm::vec3(1.0f);
		glm::vec3 emission = glm::vec3(0.0f);
		float roughness = 0.0f;
		float ior = 1.0f;
	};

	struct Submesh
	{
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t MaterialIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;

		Math::BoundingBox BoundingBox;

		glm::mat4 Transform;
		glm::mat4 MeshTransform;

		std::string Name;
	};


	class Mesh
	{
	public:
		Mesh(const std::string& filepath, MaterialInstance material);

		const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
		const Ref<BottomLevelAccelerationStructure>& GetAccelerationStructure() const { return m_AccelerationStructure; }
		const MaterialInstance& GetMaterial() const { return m_Material; }

		const Math::BoundingBox& GetBoundingBox() const { return m_Submeshes[0].BoundingBox; }

		static Ref<Mesh> Load(const std::string& filepath, MaterialInstance material = {});

		void Destroy();
	private:
		Vector<Submesh> m_Submeshes;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		Vector<Vertex> m_Vertices;
		Vector<Index> m_Indices;

		MaterialInstance m_Material;
		Vector<Texture2D> m_Albedo;

		Ref<BottomLevelAccelerationStructure> m_AccelerationStructure;

		Math::BoundingBox m_AABB;
	};
}