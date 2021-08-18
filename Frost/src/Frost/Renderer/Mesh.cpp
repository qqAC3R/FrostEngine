#include "frostpch.h"
#include "mesh.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Frost/Utils/Timer.h"

// TODO: TEMP
#include <tinyobjloader/tiny_obj_loader.h>


namespace Frost
{
	//static Ref<Texture2D> s_WhiteTexture = Texture2D::Create("assets/media/textures/white.jpg");
	//static Ref<Texture2D> s_WhiteTexture;


	Ref<Mesh> Mesh::Load(const std::string& filepath, MaterialInstance material /*= {}*/)
	{
		return CreateRef<Mesh>(filepath, material);
	}

	namespace AssimpUtils
	{
		glm::mat4 AssimpMat4ToGlmMat4(const aiMatrix4x4& matrix)
		{
			glm::mat4 result;
			result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
			result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
			result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
			result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
			return result;
		}

		void TraverseNodes(aiNode* node, Vector<Submesh>& submeshes, glm::mat4 parentTransform = glm::mat4(1.0f))
		{
			const glm::mat4 meshTransform = AssimpMat4ToGlmMat4(node->mTransformation);
			const glm::mat4 transform = parentTransform * meshTransform;

			for (uint32_t i = 0; i < node->mNumMeshes; i++)
			{
				const uint32_t mesh = node->mMeshes[i];
				auto& submesh = submeshes[mesh];
				submesh.Transform = transform;
				submesh.MeshTransform = meshTransform;
			}

			// Traverse the children of the node
			for (uint32_t i = 0; i < node->mNumChildren; i++)
				TraverseNodes(node->mChildren[i], submeshes, transform);

		}

		void SetSubmeshesAndBuffers(const aiScene* scene, Vector<Submesh>& submeshes, Vector<Vertex>& vertices, Vector<Index>& indices)
		{
			submeshes.reserve(scene->mNumMeshes);

			uint32_t vertexCount = 0;
			uint32_t indexCount = 0;

			scene->mMaterials;

			for (size_t m = 0; m < scene->mNumMeshes; m++)
			{
				aiMesh* mesh = scene->mMeshes[m];
				Submesh& submesh = submeshes.emplace_back();

				submesh.BaseVertex = vertexCount;
				submesh.BaseIndex = indexCount;
				submesh.MaterialIndex = mesh->mMaterialIndex;
				submesh.IndexCount = mesh->mNumFaces * 3;
				submesh.VertexCount = mesh->mNumVertices;
				submesh.Name = mesh->mName.C_Str();

				vertexCount += submesh.VertexCount;
				indexCount += submesh.IndexCount;

				Math::BoundingBox& aabb = submesh.BoundingBox;
				aabb.Reset();

				FROST_ASSERT(mesh->HasPositions(), "Mesh must have positions");
				FROST_ASSERT(mesh->HasNormals(), "Mesh must have normals");

				for (size_t i = 0; i < mesh->mNumVertices; i++)
				{
					Vertex vertex;

					// Settings the position and normals
					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

					// Setting bounding box
					aabb.Min.x = glm::min(vertex.Position.x, aabb.Min.x);
					aabb.Min.y = glm::min(vertex.Position.y, aabb.Min.y);
					aabb.Min.z = glm::min(vertex.Position.z, aabb.Min.z);

					aabb.Max.x = glm::max(vertex.Position.x, aabb.Max.x);
					aabb.Max.y = glm::max(vertex.Position.y, aabb.Max.y);
					aabb.Max.z = glm::max(vertex.Position.z, aabb.Max.z);

 
					if (mesh->HasTangentsAndBitangents())
					{
						// TODO: add TBN matricies
						// Currently doing nothing, but I will add TBN matricies when I get to normal mapping
					}

					if (mesh->HasTextureCoords(0))
						vertex.TexCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
					else
						vertex.TexCoords = { 0.0f, 0.0f };

					vertices.push_back(vertex);

				}

				for (size_t i = 0; i < mesh->mNumFaces; i++)
				{
					FROST_ASSERT(bool(mesh->mFaces[i].mNumIndices == 3), "Mesh must have 3 indices!");

					Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
					indices.push_back(index);
				}
			}

		}

		void SetMaterials(const aiScene* scene)
		{

		}


	}

	Mesh::Mesh(const std::string& filepath, MaterialInstance material)
	{
		//if (!s_WhiteTexture)
		//	s_WhiteTexture = Texture2D::Create("assets/media/textures/white.jpg");

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filepath, aiProcess_Triangulate | aiProcess_FlipUVs);

		if (!scene || !scene->HasMeshes())
			FROST_ASSERT(0, importer.GetErrorString());

		
		// Getting the submeshes, verticies and indices
		AssimpUtils::SetSubmeshesAndBuffers(scene, m_Submeshes, m_Vertices, m_Indices);
		
		// Traversing throw all the nodes to get the transforms of every submesh
		AssimpUtils::TraverseNodes(scene->mRootNode, m_Submeshes);

		// TO DO: Add materials later
		AssimpUtils::SetMaterials(scene);


		m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), (uint32_t)m_Vertices.size() * sizeof(Vertex));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size());

		{
			Timer timer("BLAS");

			m_AccelerationStructure = BottomLevelAccelerationStructure::Create(m_VertexBuffer, m_IndexBuffer);

		}



	}


	void Mesh::Destroy()
	{
		m_VertexBuffer->Destroy();
		m_IndexBuffer->Destroy();
		m_AccelerationStructure->Destroy();

	}

}