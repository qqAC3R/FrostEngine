#include "frostpch.h"
#include "Mesh.h"

#include "Frost/Utils/Timer.h"
#include "Frost/Renderer/Renderer.h"

#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include <cmath>
#include <filesystem>

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
		: m_Material(material), m_Filepath(filepath)
	{
		Assimp::Importer* m_Importer = new Assimp::Importer;

		const aiScene* scene = m_Importer->ReadFile(filepath, Utils::s_MeshImportFlags);
		FROST_ASSERT(!(!scene || !scene->HasMeshes()), m_Importer->GetErrorString());

		m_MeshShader = Renderer::GetShaderLibrary()->Get("GeometryPass");
		m_IsLoaded = true;

		Ref<Texture2D> whiteTexture = Renderer::GetWhiteLUT();
		// Allocate texture slots before storing the vertex data, because we are using bindless
		// We are using `scene->mNumMaterials * 4`, because each mesh has a albedo, roughness, metalness and normal map
		for (uint32_t i = 0; i < scene->mNumMaterials * 4; i++)
		{
			uint32_t textureSlot = VulkanBindlessAllocator::AddTexture(whiteTexture);
			m_TextureAllocatorSlots[i] = textureSlot;
		}


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
				
				vertex.MeshIndex = (float)mesh->mMaterialIndex;
				//vertex.MeshIndex = (float)m_TextureAllocatorSlots[mesh->mMaterialIndex * 4];


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

				

				if (subMeshLastIndex < index.V1) subMeshLastIndex = index.V1;
				if (subMeshLastIndex < index.V2) subMeshLastIndex = index.V2;
				if (subMeshLastIndex < index.V3) subMeshLastIndex = index.V3;
			}
			maxIndex = subMeshLastIndex + 1;

		}
		m_SubmeshIndexBuffers = IndexBuffer::Create(submeshIndices.data(), (uint32_t)submeshIndices.size() * sizeof(Index));


		// Traverse over every mesh to get the transforms
		TraverseNodes(scene->mRootNode);

		{
			// Timer
			std::string timerText = m_Filepath + " buffers creation";
			Timer timer(timerText.c_str());

			// Vertex/Index buffer
			m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), m_Vertices.size() * sizeof(Vertex));
			m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size() * sizeof(Index));

			// Instanced vertex buffer
			m_VertexBufferInstanced_CPU.Allocate(m_Submeshes.size() * sizeof(SubmeshInstanced) + 1);
			m_VertexBufferInstanced = BufferDevice::Create(m_Submeshes.size() * sizeof(SubmeshInstanced), { BufferUsage::Vertex });

			// Acceleration structure creation
			MeshASInfo meshInfo{};
			meshInfo.MeshVertexBuffer = m_VertexBuffer;
			meshInfo.MeshIndexBuffer = m_IndexBuffer;
			meshInfo.SubmeshIndexBuffer = m_SubmeshIndexBuffers;
			meshInfo.SubMeshes = m_Submeshes;

			m_AccelerationStructure = BottomLevelAccelerationStructure::Create(meshInfo);
		}


		if (scene->HasMaterials())
		{
			m_Textures.reserve(scene->mNumMaterials);
			m_Materials.resize(scene->mNumMaterials);
			m_MaterialData.resize(scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				// Albedo -         vec3        (12 bytes)
				// Roughness -      float       (4 bytes)
				// Metalness -      float       (4 bytes)
				// Emission -       float       (4 bytes)
				// UseNormalMap -   uint32_t    (4 bytes)
				// Texture IDs -    4 uint32_t  (16 bytes)
				m_MaterialData[i].Allocate(48);

				// Fill up the data in the correct order for us to copy it later
				DataStorage& materialData = m_MaterialData[i];
				materialData.Add("AlbedoColor",     glm::vec4(0.0f));
				materialData.Add("EmissionFactor",  0.0f);
				materialData.Add("RoughnessFactor", 0.0f);
				materialData.Add("MetalnessFactor", 0.0f);

				materialData.Add("UseNormalMap", 0);

				materialData.Add("AlbedoTexture",    0);
				materialData.Add("RoughnessTexture", 0);
				materialData.Add("MetalnessTexture", 0);
				materialData.Add("NormalTexture",    0);


				// Each mesh has 4 textures, and se we allocated numMaterials * 4 texture slots.
				uint32_t albedoTextureIndex =    (i * 4) + 0;
				uint32_t roughnessTextureIndex = (i * 4) + 1;
				uint32_t metalnessTextureIndex = (i * 4) + 2;
				uint32_t normalMapTextureIndex = (i * 4) + 3;


				m_MaterialData[i].Set("AlbedoTexture", m_TextureAllocatorSlots[albedoTextureIndex]);
				m_MaterialData[i].Set("NormalTexture", m_TextureAllocatorSlots[normalMapTextureIndex]);
				m_MaterialData[i].Set("RoughnessTexture", m_TextureAllocatorSlots[roughnessTextureIndex]);
				m_MaterialData[i].Set("MetalnessTexture", m_TextureAllocatorSlots[metalnessTextureIndex]);

				/*
				// Sometimes there could be more materials then submeshes (for some odd reason)
				if (i < m_Submeshes.size())
				{
					m_MaterialData[i].Add("ModelMatrix", m_Submeshes[i].Transform);
				}
				else
				{
					m_MaterialData[i].Add("ModelMatrix", glm::mat4(1.0f));
				}
				*/


				auto aiMaterial = scene->mMaterials[i];
				auto aiMaterialName = aiMaterial->GetName();

				auto mi = Material::Create(m_MeshShader, aiMaterialName.data);
				m_Materials[i] = mi;
				
				uint32_t textureCount = aiMaterial->GetTextureCount(aiTextureType_DIFFUSE);

				aiColor3D aiColor, aiEmission;
				// Getting the albedo color
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
				{
					glm::vec3 materialColor = { aiColor.r, aiColor.g, aiColor.b };

					mi->Set("u_MaterialUniform.AlbedoColor", materialColor);
					m_MaterialData[i].Set("AlbedoColor", glm::vec4(materialColor, 1.0f));
				}

				// Geting the emission factor
				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS)
				{
					mi->Set("u_MaterialUniform.Emission", aiEmission.r);
					m_MaterialData[i].Set("EmissionFactor", aiEmission.r);
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
				m_MaterialData[i].Set("RoughnessFactor", roughness);
				m_MaterialData[i].Set("MetalnessFactor", metalness);


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
					textureSpec.Usage = ImageUsage::ReadOnly;
					textureSpec.UseMips = true;
					auto texture = Texture2D::Create(texturePath, textureSpec);
					if (texture->Loaded())
					{
						texture->GenerateMipMaps();
						m_Textures.push_back(texture);
						mi->Set("u_AlbedoTexture", texture);
						mi->Set("u_MaterialUniform.AlbedoColor", glm::vec3(1.0f));

						VulkanBindlessAllocator::AddTextureCustomSlot(texture, m_TextureAllocatorSlots[albedoTextureIndex]);
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						mi->Set("u_AlbedoTexture", whiteTexture);
						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[albedoTextureIndex]);
					}
				}
				else
				{
					mi->Set("u_AlbedoTexture", whiteTexture);
					VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[i]);
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
					textureSpec.Usage = ImageUsage::ReadOnly;
					auto texture = Texture2D::Create(texturePath, textureSpec);
					if (texture->Loaded())
					{
						m_Textures.push_back(texture);
						mi->Set("u_NormalTexture", texture);
						mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(1));

						m_MaterialData[i].Set("UseNormalMap", uint32_t(1));
						VulkanBindlessAllocator::AddTextureCustomSlot(texture, m_TextureAllocatorSlots[normalMapTextureIndex]);
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						mi->Set("u_NormalTexture", whiteTexture);
						mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(0));

						m_MaterialData[i].Set("UseNormalMap", uint32_t(0));
						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[normalMapTextureIndex]);
					}
				}
				else
				{
					mi->Set("u_NormalTexture", whiteTexture);
					mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(0));

					m_MaterialData[i].Set("UseNormalMap", uint32_t(0));
					VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[normalMapTextureIndex]);

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
					textureSpec.Usage = ImageUsage::ReadOnly;
					auto texture = Texture2D::Create(texturePath, textureSpec);
					if (texture->Loaded())
					{
						m_Textures.push_back(texture);
						mi->Set("u_RoughnessTexture", texture);

						VulkanBindlessAllocator::AddTextureCustomSlot(texture, m_TextureAllocatorSlots[roughnessTextureIndex]);
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						mi->Set("u_RoughnessTexture", whiteTexture);

						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[roughnessTextureIndex]);
					}
				}
				else
				{
					mi->Set("u_RoughnessTexture", whiteTexture);

					VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[roughnessTextureIndex]);
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
							textureSpec.Usage = ImageUsage::ReadOnly;

							auto texture = Texture2D::Create(texturePath, textureSpec);
							if (texture->Loaded())
							{
								metalnessTextureFound = true;
								m_Textures.push_back(texture);
								mi->Set("u_MetalnessTexture", texture);

								VulkanBindlessAllocator::AddTextureCustomSlot(texture, m_TextureAllocatorSlots[metalnessTextureFound]);
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
						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, m_TextureAllocatorSlots[metalnessTextureIndex]);
					}


#if 0
					// Setting the vbo buffer device address (for not binding the vertex buffer all the time)
					mi->Set("VertexPointer", m_VertexBufferPointerBD);
#endif

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

	void Mesh::TraverseNodes(aiNode* node, const glm::mat4& parentTransform, uint32_t level)
	{
		glm::mat4 transform = parentTransform * Utils::AssimpMat4ToGlmMat4(node->mTransformation);
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			uint32_t mesh = node->mMeshes[i];
			auto& submesh = m_Submeshes[mesh];
			submesh.Transform = transform;
			//submesh.Transform = glm::mat4(1.0f);
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			TraverseNodes(node->mChildren[i], transform, level + 1);
	}

	Mesh::~Mesh()
	{
		auto whiteTexture = Renderer::GetWhiteLUT();
		for (auto& textureSlotPair : m_TextureAllocatorSlots)
		{
			uint32_t textureSlot = textureSlotPair.second;
			VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureSlot);
			VulkanBindlessAllocator::RevmoveTextureCustomSlot(textureSlot);
		}
	}

}