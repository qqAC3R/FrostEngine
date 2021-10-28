#pragma once

#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "Frost/Renderer/Texture.h"
#include "Frost/Renderer/Material.h"

#include "Frost/Math/BoundingBox.h"
#include <glm/glm.hpp>

#include "Frost/Renderer/RayTracing/AccelerationStructures.h"

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace Assimp { class Importer; }

namespace Frost
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec2 TexCoord;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
		uint32_t  MeshIndex;
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

	struct Triangle
	{
		Vertex V0, V1, V2;

		Triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2)
			: V0(v0), V1(v1), V2(v2) {}
	};

	struct Submesh
	{
		uint32_t BaseVertex;
		uint32_t BaseIndex;
		uint32_t MaterialIndex;
		uint32_t IndexCount;
		uint32_t VertexCount;

		Math::BoundingBox BoundingBox;

		glm::mat4 Transform{ 1.0f };
		glm::mat4 MeshTransform{ 1.0f };

		std::string MeshName;
	};

	struct MaterialUniform
	{
		glm::vec3 AlbedoColor = glm::vec3(0.8f);
		float Emission = 0.0f;
		float Roughness = 0.0f;
		float Metalness = 0.0f;
		bool UseNormalMap = false;
	};

	class Mesh
	{
	public:
		Mesh(const std::string& filepath, MaterialInstance material);
		~Mesh();

		const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
		const Ref<IndexBuffer>& GetSubmeshIndexBuffer() const { return m_SubmeshIndexBuffers; }
		Ref<BottomLevelAccelerationStructure> GetAccelerationStructure() const { return m_AccelerationStructure; }
		const MaterialInstance& GetMaterial() const { return m_Material; }
		MaterialInstance& GetMaterial() { return m_Material; }
		Vector<Ref<Material>> GetVulkanMaterial() { return m_Materials; }

		const Vector<Submesh> GetSubMeshes() const { return m_Submeshes; }

		const Math::BoundingBox& GetBoundingBox() const { return m_BoundingBox; }

		static Ref<Mesh> Load(const std::string& filepath, MaterialInstance material = {});

		void Destroy();
	private:
		void TraverseNodes(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);
	private:
		Assimp::Importer* m_Importer;
		Ref<Shader> m_MeshShader;
		
		Math::BoundingBox m_BoundingBox;
		Vector<Submesh> m_Submeshes;
		Vector<Vertex> m_Vertices;
		Vector<Index> m_Indices;
		HashMap<uint32_t, Vector<Triangle>> m_TriangleCache;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		Ref<IndexBuffer> m_SubmeshIndexBuffers;

		Ref<UniformBuffer> m_MaterialUniforms;
		Vector<Ref<Texture2D>> m_Textures;
		Vector<Ref<Material>> m_Materials;

		MaterialInstance m_Material;
		Ref<BottomLevelAccelerationStructure> m_AccelerationStructure;
	};
}