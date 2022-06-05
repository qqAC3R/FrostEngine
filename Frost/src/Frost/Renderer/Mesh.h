#pragma once

#include "Frost/Renderer/Buffers/VertexBuffer.h"
#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "Frost/Renderer/Texture.h"
#include "Frost/Renderer/Material.h"

#include "Frost/Math/BoundingBox.h"
#include <glm/glm.hpp>

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace Frost
{
	struct Vertex
	{
		glm::vec3 Position;
		glm::vec2 TexCoord;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec3 Bitangent;
		float     MeshIndex;
	};

	struct Index
	{
		uint32_t V1, V2, V3;
	};

	// Used by RT for now, tho I'll need to change it
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

	struct SubmeshInstanced
	{
		glm::mat4 ModelSpaceMatrix;
		glm::mat4 WorldSpaceMatrix;
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
		virtual ~Mesh();

		const Ref<VertexBuffer>& GetVertexBuffer() const { return m_VertexBuffer; }
		const Ref<BufferDevice>& GetVertexBufferInstanced(uint32_t index) const { return m_VertexBufferInstanced[index]; }
		const Ref<IndexBuffer>& GetIndexBuffer() const { return m_IndexBuffer; }
		const Ref<IndexBuffer>& GetSubmeshIndexBuffer() const { return m_SubmeshIndexBuffers; }

		Vector<Vertex>& GetVertices() { return m_Vertices; }
		const Vector<Vertex>& GetVertices() const { return m_Vertices; }

		Vector<Index>& GetIndices() { return m_Indices; }
		const Vector<Index>& GetIndices() const { return m_Indices; }

		const Vector<Submesh>& GetSubMeshes() const { return m_Submeshes; }

		Buffer& GetVertexBufferInstanced_CPU(uint32_t index) { return m_VertexBufferInstanced_CPU[index]; }
		void UpdateInstancedVertexBuffer(const glm::mat4& transform, const glm::mat4& viewProjMatrix, uint32_t currentFrameIndex);

		uint32_t GetMaterialCount() { return (uint32_t)m_MaterialData.size(); }
		DataStorage& GetMaterialData(uint32_t materialIndex) { return m_MaterialData[materialIndex]; }

		Ref<Texture2D> GetTexture(uint32_t textureId) { return m_Textures[textureId]; }

		MaterialInstance& GetMaterial() { return m_Material; } // TODO: Remove this
		const MaterialInstance& GetMaterial() const { return m_Material; } // TODO: Remove this
		Vector<Ref<Material>> GetVulkanMaterial() { return m_Materials; } // TODO: Remove this (now using bindless)
		Ref<BottomLevelAccelerationStructure> GetAccelerationStructure() const { return m_AccelerationStructure; }

		bool IsLoaded() const { return m_IsLoaded; }
		const std::string& GetFilepath() const { return m_Filepath; }

		void SetNewTexture(uint32_t textureId, Ref<Texture2D> texture);

		static Ref<Mesh> Load(const std::string& filepath, MaterialInstance material = {});
	private:
		void TraverseNodes(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);
	private:
		Ref<Shader> m_MeshShader;

		std::string m_Filepath;
		bool m_IsLoaded;
		
		Vector<Vertex> m_Vertices;
		Vector<Index> m_Indices;
		Vector<Submesh> m_Submeshes;
		HashMap<uint32_t, Vector<Triangle>> m_TriangleCache;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		Ref<IndexBuffer> m_SubmeshIndexBuffers;

		Vector<Buffer> m_VertexBufferInstanced_CPU;
		Vector<Ref<BufferDevice>> m_VertexBufferInstanced;

		//Vector<Ref<Texture2D>> m_Textures;
		HashMap<uint32_t, Ref<Texture2D>> m_Textures;
		Vector<Ref<Material>> m_Materials; // TODO: Maybe remove it? since the engine is now bindless
		Vector<DataStorage> m_MaterialData; // Bindless data

		HashMap<uint32_t, uint32_t> m_TextureAllocatorSlots; // Bindless
		Ref<BottomLevelAccelerationStructure> m_AccelerationStructure; // For RT




		MaterialInstance m_Material;
	};
}