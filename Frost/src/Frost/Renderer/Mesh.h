#pragma once

#include "Frost/Renderer/Texture.h"
#include "Frost/Renderer/Material.h"
#include "Frost/Renderer/Animation.h"
#include "Frost/Renderer/Buffers/IndexBuffer.h"
#include "Frost/Renderer/Buffers/VertexBuffer.h"

#include "Frost/Math/BoundingBox.h"
#include <glm/glm.hpp>

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace Assimp
{
	class Importer;
}

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

	struct AnimatedVertex
	{
		glm::vec3  Position;
		glm::vec2  TexCoord;
		glm::vec3  Normal;
		glm::vec3  Tangent;
		glm::vec3  Bitangent;
		float      MeshIndex;

		int32_t IDs[4] = { 0, 0, 0, 0 };
		float Weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		

		void AddBoneData(uint32_t BoneID, float Weight)
		{
			for (uint32_t i = 0; i < 4; i++)
			{
				if (Weights[i] == 0.0)
				{
					IDs[i] = BoneID;
					Weights[i] = Weight;
					return;
				}
			}

			FROST_CORE_WARN("Vertex has more than four bones/weights affecting it, extra data will be discarded (BoneID={0}, Weight={1})", BoneID, Weight);
		}
	};

	struct Index
	{
		uint32_t V1, V2, V3;
	};

	struct BoneInfo
	{
		glm::mat4 InverseBindPose;
		uint32_t JointIndex;

		BoneInfo(glm::mat4 inverseBindPose, uint32_t jointIndex) : InverseBindPose(inverseBindPose), JointIndex(jointIndex) {}
	};

	// Used by RT for now, tho I'll need to change it
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
	private:
		// The constructor is private for several reasons, firstly beacuse here we pass some custom settings for the mesh to be built,
		// and that should be done only internally. The user shouldn't access any of that, instead there is the `Load` function which just requires the filepath
		struct MeshBuildSettings;
		Mesh(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings = {});
	public:
		virtual ~Mesh();

		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		Ref<BufferDevice> GetVertexBufferInstanced(uint32_t index) const { return m_VertexBufferInstanced[index]; }
		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }
		Ref<UniformBuffer> GetBoneUniformBuffer(uint32_t currentFrameIndex) const { return m_BoneTransformsUniformBuffer[currentFrameIndex]; }
		
		Vector<Vertex>& GetVertices() { return m_Vertices; }
		const Vector<Vertex>& GetVertices() const { return m_Vertices; }

		Vector<Index>& GetIndices() { return m_Indices; }
		const Vector<Index>& GetIndices() const { return m_Indices; }

		const Vector<Submesh>& GetSubMeshes() const { return m_Submeshes; }
		const Vector<Ref<Animation>>& GetAnimations() const { return m_Animations; }

		Buffer& GetVertexBufferInstanced_CPU(uint32_t index) { return m_VertexBufferInstanced_CPU[index]; }
		void UpdateInstancedVertexBuffer(const glm::mat4& transform, const glm::mat4& viewProjMatrix, uint32_t currentFrameIndex);
		//void Update(float deltaTime);
		void UpdateBoneTransformMatrices(const ozz::vector<ozz::math::Float4x4>& modelSpaceMatrices);

		uint32_t GetMaterialCount() { return (uint32_t)m_MaterialData.size(); }
		DataStorage& GetMaterialData(uint32_t materialIndex) { return m_MaterialData[materialIndex]; }

		Ref<Texture2D> GetTexture(uint32_t textureId) { return m_Textures[textureId]; }

		MaterialInstance& GetMaterial() { return m_Material; } // TODO: Remove this
		const MaterialInstance& GetMaterial() const { return m_Material; } // TODO: Remove this

		Ref<BottomLevelAccelerationStructure> GetAccelerationStructure() const { return m_AccelerationStructure; }
		Ref<IndexBuffer> GetSubmeshIndexBuffer() const { return m_SubmeshIndexBuffers; }

		bool IsLoaded() const { return m_IsLoaded; }
		bool IsAnimated() const { return m_IsAnimated; }
		const std::string& GetFilepath() const { return m_Filepath; }

		void SetNewTexture(uint32_t textureId, Ref<Texture2D> texture);

		static Ref<Mesh> Load(const std::string& filepath, MaterialInstance material = {});
	private:
		void TraverseNodes(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);

		static Ref<Mesh> LoadCustomMesh(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings = {});
	private:
		std::string m_Filepath;
		bool m_IsLoaded;
		bool m_IsAnimated;
		
		// Assimp import helpers
		Scope<Assimp::Importer> m_Importer;
		const aiScene* m_Scene;

		// Mesh data
		Vector<Vertex> m_Vertices;
		Vector<AnimatedVertex> m_SkinnedVertices;
		Vector<Index> m_Indices;
		Vector<Submesh> m_Submeshes;

		// Bone/Animation information
		uint32_t m_BoneCount = 0;
		Vector<BoneInfo> m_BoneInfo;
		Ref<MeshSkeleton> m_Skeleton;
		Vector<Ref<Animation>> m_Animations;
		Vector<Ref<UniformBuffer>> m_BoneTransformsUniformBuffer;
		Vector<glm::mat4> m_BoneTransforms;
		

		// Vertex and Index GPU buffers 
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		// For GPU Driven Renderer
		Vector<Buffer> m_VertexBufferInstanced_CPU;       
		Vector<Ref<BufferDevice>> m_VertexBufferInstanced;

		// Textures, stored in a ID fashioned way, so it is easier to be supported by the bindless renderer design
		HashMap<uint32_t, Ref<Texture2D>> m_Textures;
		HashMap<uint32_t, uint32_t> m_TextureAllocatorSlots; // Bindless
		Vector<DataStorage> m_MaterialData; // Bindless data

		struct TextureMaterialFilepaths;
		Vector<TextureMaterialFilepaths> m_TexturesFilepaths;


		// Acceleration structure + custom index buffer (Ray Tracing)
		Ref<BottomLevelAccelerationStructure> m_AccelerationStructure;
		Ref<IndexBuffer> m_SubmeshIndexBuffers; // Same index buffer, but all grouped into a single mesh

		struct TextureMaterialFilepaths
		{
			std::string AlbedoFilepath = "";
			std::string NormalMapFilepath = "";
			std::string RoughnessMapFilepath = "";
			std::string MetalnessMapFilepath = "";
		};

		struct MeshBuildSettings
		{
			bool LoadMaterials = true; // Mostly for serialization (it loads materials internally)
			bool CreateBottomLevelStructure = true; // For ray tracing
		};

		MaterialInstance m_Material; // TODO: Remove


		friend class Animation;
		friend class SceneSerializer;
		friend class Ref<Mesh>;
	};
}