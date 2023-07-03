#pragma once

#include "Frost/Asset/Asset.h"

#include "Frost/Renderer/MaterialAsset.h"
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

	// Here we should keep the meshes that are loaded once with the engine
	struct DefaultMeshStorage
	{
		Ref<MeshAsset> Cube;
		Ref<MeshAsset> Sphere;
		Ref<MeshAsset> Capsule;
	};

	class MeshAsset : public Asset
	{
	private:
		// The constructor is private for several reasons, firstly beacuse here we pass some custom settings for the mesh to be built,
		// and that should be done only internally. The user shouldn't access any of that, instead there is the `Load` function which just requires the filepath
		struct MeshBuildSettings;
		MeshAsset(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings = {});
		MeshAsset(const Vector<Vertex>& vertices, const Vector<Index>& indices, const glm::mat4& transform);
	public:
		MeshAsset() = default; // Constructor which basically does nothing
		virtual ~MeshAsset();

		Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		///A Ref<BufferDevice> GetVertexBufferInstanced(uint32_t index) const { return m_VertexBufferInstanced[index]; }
		Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }
		///A Ref<UniformBuffer> GetBoneUniformBuffer(uint32_t currentFrameIndex) const { return m_BoneTransformsUniformBuffer[currentFrameIndex]; }
		
		Vector<Vertex>& GetVertices() { return m_Vertices; }
		const Vector<Vertex>& GetVertices() const { return m_Vertices; }

		Vector<Index>& GetIndices() { return m_Indices; }
		const Vector<Index>& GetIndices() const { return m_Indices; }

		const Vector<Submesh>& GetSubMeshes() const { return m_Submeshes; }
		const Vector<Ref<Animation>>& GetAnimations() const { return m_Animations; }

		static AssetType GetStaticType() { return AssetType::MeshAsset; }
		virtual AssetType GetAssetType() const override { return AssetType::MeshAsset; }
		virtual bool ReloadData(const std::string& filepath) override;

		///A Buffer& GetVertexBufferInstanced_CPU(uint32_t index) { return m_VertexBufferInstanced_CPU[index]; }
		///A void UpdateInstancedVertexBuffer(const glm::mat4& transform, const glm::mat4& viewProjMatrix, uint32_t currentFrameIndex);
		//void Update(float deltaTime);
		///A void UpdateBoneTransformMatrices(const ozz::vector<ozz::math::Float4x4>& modelSpaceMatrices);

		///A uint32_t GetMaterialCount() { return (uint32_t)m_MaterialData.size(); }
		///A DataStorage& GetMaterialData(uint32_t materialIndex) { return m_MaterialData[materialIndex]; }

		///A Ref<Texture2D> GetTexture(uint32_t textureId) { return m_Textures[textureId]; }

		MaterialInstance& GetMaterial() { return m_Material; } // TODO: Remove this
		const MaterialInstance& GetMaterial() const { return m_Material; } // TODO: Remove this

		Ref<BottomLevelAccelerationStructure> GetAccelerationStructure() const { return m_AccelerationStructure; }
		Ref<IndexBuffer> GetSubmeshIndexBuffer() const { return m_SubmeshIndexBuffers; }

		bool IsLoaded() const { return m_IsLoaded; }
		bool IsAnimated() const { return m_IsAnimated; }
		const std::string& GetFilepath() const { return m_Filepath; }

		///A void SetNewTexture(uint32_t textureId, Ref<Texture2D> texture);

		static const DefaultMeshStorage& GetDefaultMeshes();
		static Ref<MeshAsset> Load(const std::string& filepath, MaterialInstance material = {});
	private:
		void TraverseNodes(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);

		static Ref<MeshAsset> LoadCustomMesh(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings = {});

		static void InitDefaultMeshes();
		static void DestroyDefaultMeshes();
	private:
		std::string m_Filepath;
		bool m_IsLoaded = false;
		bool m_IsAnimated = false;
		
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
		//Vector<Ref<UniformBuffer>> m_BoneTransformsUniformBuffer;
		//Vector<glm::mat4> m_BoneTransforms;
		

		// Vertex and Index GPU buffers 
		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		// For GPU Driven Renderer
		//Vector<Buffer> m_VertexBufferInstanced_CPU;
		//Vector<Ref<BufferDevice>> m_VertexBufferInstanced;


		// Currently, those materials/textures are read only,
		// however in the `Mesh` class all the values and textures are copied into a new buffer
		// and from there on the user can change the values.
		// But those variables remain the same for the entirety of the application's runtime
		// NOTE: Textures are stored in a ID fashioned way, so it is easier to be supported by the bindless renderer design
		Vector<Ref<Texture2D>> m_TexturesList;
		Vector<DataStorage> m_MaterialData; // Bindless data
		Vector<std::string> m_MaterialNames;

		// Acceleration structure + custom index buffer (Ray Tracing)
		Ref<BottomLevelAccelerationStructure> m_AccelerationStructure;
		Ref<IndexBuffer> m_SubmeshIndexBuffers; // Same index buffer, but all grouped into a single mesh (for RT)

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
		friend class CookingFactory;
		friend class Renderer;
		friend struct MaterialMeshPointer;
		friend class Ref<MeshAsset>;
		friend class Mesh;
	};


	// Dynamic Submeshes (inspired from UE4)
	struct SkeletalSubmesh
	{
		glm::mat4 Transform;
	};

	class Mesh
	{
	public:
		explicit Mesh(Ref<MeshAsset> meshAsset);
		virtual ~Mesh();

		Ref<BufferDevice> GetVertexBufferInstanced(uint32_t index) const { return m_VertexBufferInstanced[index]; }
		Buffer& GetVertexBufferInstanced_CPU(uint32_t index) { return m_VertexBufferInstanced_CPU[index]; }

		void UpdateBoneTransformMatrices(const ozz::vector<ozz::math::Float4x4>& modelSpaceMatrices);

		Ref<UniformBuffer> GetBoneUniformBuffer(uint32_t currentFrameIndex) const { return m_BoneTransformsUniformBuffer[currentFrameIndex]; }

		bool IsLoaded() const { return m_MeshAsset->IsLoaded(); }
		bool IsAnimated() const { return m_MeshAsset->IsAnimated(); }

		const Vector<SkeletalSubmesh>& GetSkeletalSubmeshes() const { return m_Submeshes; }

		const Ref<MeshAsset>& GetMeshAsset() const { return m_MeshAsset; }

		uint32_t GetMaterialCount() const { return (uint32_t)m_MaterialAssets.size(); }
		Ref<MaterialAsset> GetMaterialAsset(uint32_t materialIndex) { return m_MaterialAssets[materialIndex]; }
		void SetMaterialAssetToDefault(uint32_t materialIndex);

		void SetNewTexture(uint32_t materialIndex, uint32_t textureId, Ref<Texture2D> texture);
		Ref<Texture2D> GetTexture(uint32_t materialIndex, uint32_t textureId) { return m_MaterialAssets[materialIndex]->GetTextureById(textureId); }

		void SetMaterialByAsset(uint32_t index, Ref<MaterialAsset> materialAsset);
	private:
		Ref<MeshAsset> m_MeshAsset;

		// This submesh information should be dynamic, it means you can change the transforms, unlike from the mesh asset which are only read-onlu
		Vector<SkeletalSubmesh> m_Submeshes;
		
		// Textures, stored in a ID fashioned way, so it is easier to be supported by the bindless renderer design
		Vector<Ref<MaterialAsset>> m_MaterialAssets;

		// For animations
		Vector<Ref<UniformBuffer>> m_BoneTransformsUniformBuffer;
		Vector<glm::mat4> m_BoneTransforms;

		// For GPU Driven Renderer
		Vector<Buffer> m_VertexBufferInstanced_CPU;
		Vector<Ref<BufferDevice>> m_VertexBufferInstanced;


		friend struct MaterialMeshPointer;
		friend class MeshAsset;
	};
}