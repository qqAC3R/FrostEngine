#include "frostpch.h"
#include "mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cmath>
#include <filesystem>

#include "Frost/Utils/Timer.h"
#include "Frost/Renderer/Renderer.h"

namespace Frost
{
	
	Ref<Mesh> Mesh::Load(const std::string& filepath, MaterialInstance material /*= {}*/)
	{
		return CreateRef<Mesh>(filepath, material);
	}

	namespace Utils
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

		static const uint32_t s_MeshImportFlags =
			aiProcess_CalcTangentSpace |        // Create binormals/tangents just in case
			aiProcess_Triangulate |             // Make sure we're triangles
			aiProcess_SortByPType |             // Split meshes by primitive type
			aiProcess_GenNormals |              // Make sure we have legit normals
			aiProcess_GenUVCoords |             // Convert UVs if required 
			aiProcess_OptimizeMeshes |          // Batch draws where possible
			aiProcess_JoinIdenticalVertices |
			aiProcess_ValidateDataStructure;    // Validation

	}

	Mesh::Mesh(const std::string& filepath, MaterialInstance material)
		: m_Material(material)
	{
		m_Importer = new Assimp::Importer;

		const aiScene* scene = m_Importer->ReadFile(filepath, Utils::s_MeshImportFlags);
		FROST_ASSERT(!(!scene || !scene->HasMeshes()), m_Importer->GetErrorString());

		m_MeshShader = Renderer::GetShaderLibrary()->Get("GeometryPass");

		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		Vector<Index> submeshIndices;
		uint32_t maxIndex = 0;
		m_Submeshes.reserve(scene->mNumMeshes);
		for (unsigned m = 0; m < scene->mNumMeshes; m++)
		{
			aiMesh* mesh = scene->mMeshes[m];

			Submesh& submesh = m_Submeshes.emplace_back();
			submesh.BaseVertex = vertexCount;             // Vertices information
			submesh.VertexCount = mesh->mNumVertices;     // Vertices information
			submesh.BaseIndex = indexCount;				  // Indices information
			submesh.IndexCount = mesh->mNumFaces * 3;     // Indices information
			submesh.MaterialIndex = mesh->mMaterialIndex;
			submesh.MeshName = mesh->mName.C_Str();

			vertexCount += mesh->mNumVertices;
			indexCount += submesh.IndexCount;

			FROST_ASSERT(mesh->HasPositions(), "Meshes require positions.");
			FROST_ASSERT(mesh->HasNormals(), "Meshes require normals.");

			//auto aabb = submesh.BoundingBox;
			Math::BoundingBox aabb;
			aabb.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
			aabb.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
			for (size_t i = 0; i < mesh->mNumVertices; i++)
			{
				Vertex vertex;
				vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
				vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
				vertex.MeshIndex = mesh->mMaterialIndex;

				aabb.Min.x = glm::min(vertex.Position.x, aabb.Min.x);
				aabb.Min.y = glm::min(vertex.Position.y, aabb.Min.y);
				aabb.Min.z = glm::min(vertex.Position.z, aabb.Min.z);
				aabb.Max.x = glm::max(vertex.Position.x, aabb.Max.x);
				aabb.Max.y = glm::max(vertex.Position.y, aabb.Max.y);
				aabb.Max.z = glm::max(vertex.Position.z, aabb.Max.z);

				// Setting up the bounding box into the submesh
				submesh.BoundingBox = Math::BoundingBox(aabb.Min, aabb.Max);

				if (mesh->HasTangentsAndBitangents())
				{
					vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
					vertex.Bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
				}

				if (mesh->HasTextureCoords(0))
					vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
				else
					vertex.TexCoord = { 0.0f, 0.0f };

				m_Vertices.push_back(vertex);
			}

			uint32_t subMeshLastIndex = 0;

			// Indices
			for (size_t i = 0; i < mesh->mNumFaces; i++)
			{
				FROST_ASSERT(bool(mesh->mFaces[i].mNumIndices == 3), "Must have 3 indices.");
				Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
				m_Indices.push_back(index);

				m_TriangleCache[m].emplace_back(
					m_Vertices[index.V1 + submesh.BaseVertex], m_Vertices[index.V2 + submesh.BaseVertex], m_Vertices[index.V3 + submesh.BaseVertex]
				);

				// ------------------------------
				index = { maxIndex + mesh->mFaces[i].mIndices[0], maxIndex + mesh->mFaces[i].mIndices[1], maxIndex + mesh->mFaces[i].mIndices[2] };
				submeshIndices.push_back(index);

				

				if (subMeshLastIndex < index.V1)
					subMeshLastIndex = index.V1;

				if (subMeshLastIndex < index.V2)
					subMeshLastIndex = index.V2;

				if (subMeshLastIndex < index.V3)
					subMeshLastIndex = index.V3;
			}
			maxIndex = subMeshLastIndex + 1;

		}
		m_SubmeshIndexBuffers = IndexBuffer::Create(submeshIndices.data(), (uint32_t)submeshIndices.size() * sizeof(Index));


		// Traverse over every mesh to get the transforms
		TraverseNodes(scene->mRootNode);

		// Calculate AABB for every submesh
		for (const auto& submesh : m_Submeshes)
		{
			Math::BoundingBox transformedSubmeshAABB = submesh.BoundingBox;
			glm::vec3 min = glm::vec3(submesh.Transform * glm::vec4(transformedSubmeshAABB.Min, 1.0f));
			glm::vec3 max = glm::vec3(submesh.Transform * glm::vec4(transformedSubmeshAABB.Max, 1.0f));

			m_BoundingBox.Min.x = glm::min(m_BoundingBox.Min.x, min.x);
			m_BoundingBox.Min.y = glm::min(m_BoundingBox.Min.y, min.y);
			m_BoundingBox.Min.z = glm::min(m_BoundingBox.Min.z, min.z);
			m_BoundingBox.Max.x = glm::max(m_BoundingBox.Max.x, max.x);
			m_BoundingBox.Max.y = glm::max(m_BoundingBox.Max.y, max.y);
			m_BoundingBox.Max.z = glm::max(m_BoundingBox.Max.z, max.z);
		}

		{
			Timer timer("Mesh's buffers creation");

			m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), m_Vertices.size() * sizeof(Vertex));
			m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size() * sizeof(Index));

			MeshASInfo meshInfo{};
			meshInfo.MeshVertexBuffer = m_VertexBuffer;
			meshInfo.MeshIndexBuffer = m_IndexBuffer;
			meshInfo.SubmeshIndexBuffer = m_SubmeshIndexBuffers;
			meshInfo.SubMeshes = m_Submeshes;

			m_AccelerationStructure = BottomLevelAccelerationStructure::Create(meshInfo);
		}


		// Materials
		m_MaterialUniforms = UniformBuffer::Create(sizeof(MaterialUniform));

		Ref<Texture2D> whiteTexture = Renderer::GetWhiteLUT();
		if (scene->HasMaterials())
		{
			m_Textures.reserve(scene->mNumMaterials);
			m_Materials.resize(scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				auto aiMaterial = scene->mMaterials[i];
				auto aiMaterialName = aiMaterial->GetName();

				auto mi = Material::Create(m_MeshShader, aiMaterialName.data);
				m_Materials[i] = mi;
				
				uint32_t textureCount = aiMaterial->GetTextureCount(aiTextureType_DIFFUSE);

				aiColor3D aiColor, aiEmission;
				// Getting the albedo color
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
				{
					mi->Set("u_MaterialUniform.AlbedoColor", { aiColor.r, aiColor.g, aiColor.b });
				}

				// Geting the emission factor
				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS)
				{
					mi->Set("u_MaterialUniform.Emission", aiEmission.r);
				}


				float shininess, metalness;
				// Getting the shininess/roughness factor
				if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
					shininess = 80.0f; // Default value

				// Getting the metalness factor
				if (aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness) != aiReturn_SUCCESS)
					metalness = 0.0f;

				// Clamping the values
				float roughness = 1.0f - glm::sqrt(shininess / 100.0f);
				if (roughness == 1.0f) roughness = 0.99f;

				mi->Set("u_MaterialUniform.Roughness", roughness);
				mi->Set("u_MaterialUniform.Metalness", metalness);


				// Albedo Map
				aiString aiTexPath;
				bool hasAlbedoMap = aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS;
				if (hasAlbedoMap)
				{
					std::filesystem::path path = filepath;
					auto parentPath = path.parent_path();
					parentPath /= std::string(aiTexPath.data);
					std::string texturePath = parentPath.string();

					TextureSpecification textureSpec{};
					textureSpec.Format = ImageFormat::RGBA8;
					textureSpec.Usage = ImageUsage::Storage;
					textureSpec.UseMips = true;
					auto texture = Texture2D::Create(texturePath, textureSpec);
					if (texture->Loaded())
					{
						texture->GenerateMipMaps();
						m_Textures.push_back(texture);
						mi->Set("u_AlbedoTexture", texture);
						mi->Set("u_MaterialUniform.AlbedoColor", glm::vec3(1.0f));
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						mi->Set("u_AlbedoTexture", whiteTexture);
					}
				}
				else
				{
					mi->Set("u_AlbedoTexture", whiteTexture);
				}

				// Normal map
				bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiTexPath) == AI_SUCCESS;
				if (hasNormalMap)
				{
					std::filesystem::path path = filepath;
					auto parentPath = path.parent_path();
					parentPath /= std::string(aiTexPath.data);
					std::string texturePath = parentPath.string();

					TextureSpecification textureSpec{};
					textureSpec.Format = ImageFormat::RGBA8;
					textureSpec.Usage = ImageUsage::Storage;
					auto texture = Texture2D::Create(texturePath, textureSpec);
					if (texture->Loaded())
					{
						m_Textures.push_back(texture);
						mi->Set("u_NormalTexture", texture);
						mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(1));
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						mi->Set("u_NormalTexture", whiteTexture);
						mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(0));
					}
				}
				else
				{
					mi->Set("u_NormalTexture", whiteTexture);
					mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(0));
				}


				// Roughness map
				bool hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
				if (hasRoughnessMap)
				{
					std::filesystem::path path = filepath;
					auto parentPath = path.parent_path();
					parentPath /= std::string(aiTexPath.data);
					std::string texturePath = parentPath.string();

					TextureSpecification textureSpec{};
					textureSpec.Format = ImageFormat::RGBA8;
					textureSpec.Usage = ImageUsage::Storage;
					auto texture = Texture2D::Create(texturePath, textureSpec);
					if (texture->Loaded())
					{
						m_Textures.push_back(texture);
						mi->Set("u_RoughnessTexture", texture);
						//materialInfo.Roughness = 1.0f;
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						mi->Set("u_RoughnessTexture", whiteTexture);
					}
				}
				else
				{
					mi->Set("u_RoughnessTexture", whiteTexture);
				}

				bool metalnessTextureFound = false;
				for (uint32_t p = 0; p < aiMaterial->mNumProperties; p++)
				{
					auto prop = aiMaterial->mProperties[p];

					if (prop->mType == aiPTI_String)
					{
						uint32_t strLength = *(uint32_t*)prop->mData;
						std::string str(prop->mData + 4, strLength);

						std::string key = prop->mKey.data;
						if (key == "$raw.ReflectionFactor|file")
						{
							std::filesystem::path path = filepath;
							auto parentPath = path.parent_path();
							parentPath /= std::string(aiTexPath.data);
							std::string texturePath = parentPath.string();

							TextureSpecification textureSpec{};
							textureSpec.Format = ImageFormat::RGBA8;
							textureSpec.Usage = ImageUsage::Storage;

							auto texture = Texture2D::Create(texturePath, textureSpec);
							if (texture->Loaded())
							{
								metalnessTextureFound = true;
								m_Textures.push_back(texture);
								mi->Set("u_MetalnessTexture", texture);
								//materialInfo.Metalness = 1.0f;
							}
							else
							{
								FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
							}
							break;
						}
					}

					if (!metalnessTextureFound)
					{
						mi->Set("u_MetalnessTexture", whiteTexture);
					}

				}
			}
		}
		else
		{
			auto mi = Material::Create(m_MeshShader, "MeshShader-Default");
			mi->Set("u_AlbedoTexture", whiteTexture);
			mi->Set("u_NormalTexture", whiteTexture);
			mi->Set("u_MetalnessTexture", whiteTexture);

			mi->Set("u_MaterialUniform.AlbedoColor", glm::vec3(0.8f));
			mi->Set("u_MaterialUniform.Emission", 0.0f);
			mi->Set("u_MaterialUniform.Roughness", 0.0f);
			mi->Set("u_MaterialUniform.Metalness", 0.0f);
			mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(0));
		}

		return;
	}


	Mesh::~Mesh()
	{
		Destroy();
	}

	void Mesh::TraverseNodes(aiNode* node, const glm::mat4& parentTransform, uint32_t level)
	{
		glm::mat4 transform = parentTransform * Utils::AssimpMat4ToGlmMat4(node->mTransformation);
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			uint32_t mesh = node->mMeshes[i];
			auto& submesh = m_Submeshes[mesh];
			//submesh.Transform = transform;
			submesh.Transform = glm::mat4(1.0f);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			TraverseNodes(node->mChildren[i], transform, level + 1);
	}

	void Mesh::Destroy()
	{
	}
}