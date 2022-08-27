#include "frostpch.h"
#include "Mesh.h"

#include "Frost/Utils/Timer.h"
#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/Animation.h"
#include "Frost/Renderer/OZZAssimpImporter.h"

#include "Frost/Platform/Vulkan/VulkanBindlessAllocator.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/base/maths/simd_math.h>

#include <glm/gtc/type_ptr.hpp>

//#include <cmath>
#include <filesystem>

#define MAX_BONES 400

namespace Frost
{

	Ref<Mesh> Mesh::Load(const std::string& filepath, MaterialInstance material /*= {}*/)
	{
		return CreateRef<Mesh>(filepath, material);
	}

	Ref<Mesh> Mesh::LoadCustomMesh(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings /*= {}*/)
	{
		return CreateRef<Mesh>(filepath, material, meshBuildSettings);
	}

	namespace Utils
	{
		static glm::mat4 AssimpMat4ToGlmMat4(const aiMatrix4x4& matrix)
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
			aiProcess_LimitBoneWeights |        // If more than N (=4) bone weights, discard least influencing bones and renormalise sum to 1
			aiProcess_ValidateDataStructure;    // Validation

	}

	Mesh::Mesh(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings)
		: m_Material(material), m_Filepath(filepath)
	{
		m_Importer = CreateScope<Assimp::Importer>();

		const aiScene* scene = m_Importer->ReadFile(filepath, Utils::s_MeshImportFlags);
		FROST_ASSERT(!(!scene || !scene->HasMeshes()), m_Importer->GetErrorString());

		m_Scene = scene;
		m_IsLoaded = true;
		m_IsAnimated = scene->mAnimations != nullptr;

		if (m_IsAnimated)
		{
			m_Skeleton = CreateRef<MeshSkeleton>(scene);
			//m_AnimationController = CreateRef<AnimationController>();
		}



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
				if (m_IsAnimated)
				{
					AnimatedVertex vertex;
					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

					vertex.MeshIndex = (float)mesh->mMaterialIndex;

					if (mesh->HasTangentsAndBitangents())
					{
						vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
						vertex.Bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
					}

					if (mesh->HasTextureCoords(0))
						vertex.TexCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
					else
						vertex.TexCoord = { 0.0f, 0.0f };

					m_SkinnedVertices.push_back(vertex);
				}
				else
				{
					Vertex vertex;
					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

					vertex.MeshIndex = (float)mesh->mMaterialIndex;

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
			}

			uint32_t subMeshLastIndex = 0;

			// Indices
			for (size_t i = 0; i < mesh->mNumFaces; i++)
			{
				FROST_ASSERT(bool(mesh->mFaces[i].mNumIndices == 3), "Must have 3 indices.");
				Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
				m_Indices.push_back(index);

				//m_TriangleCache[m].emplace_back(
				//	m_Vertices[index.V1 + submesh.BaseVertex], m_Vertices[index.V2 + submesh.BaseVertex], m_Vertices[index.V3 + submesh.BaseVertex]
				//);

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


		// Bones
		if (m_IsAnimated)
		{
			const ozz::animation::Skeleton& skeleton = m_Skeleton->GetInternalSkeleton();

			for (size_t m = 0; m < scene->mNumMeshes; m++)
			{
				aiMesh* mesh = scene->mMeshes[m];
				Submesh& submesh = m_Submeshes[m];

				for (size_t i = 0; i < mesh->mNumBones; i++)
				{
					aiBone* bone = mesh->mBones[i];
					std::string boneName(bone->mName.data);

					// Find bone in skeleton
					uint32_t jointIndex = ~0;
					for (size_t j = 0; j < skeleton.joint_names().size(); j++)
					{
						if (boneName == skeleton.joint_names()[j])
						{
							jointIndex = static_cast<int>(j);
							break;
						}
					}

					if (jointIndex == ~0)
					{
						FROST_CORE_ERROR("Could not find mesh bone '{}' in skeleton!", boneName);
					}

					uint32_t boneIndex = ~0;
					for (size_t j = 0; j < m_BoneInfo.size(); ++j)
					{
						if (m_BoneInfo[j].JointIndex == jointIndex)
						{
							boneIndex = static_cast<uint32_t>(j);
							break;
						}
					}
					if (boneIndex == ~0)
					{
						// Allocate an index for a new bone
						boneIndex = static_cast<uint32_t>(m_BoneInfo.size());
						m_BoneInfo.emplace_back(Utils::AssimpMat4ToGlmMat4(bone->mOffsetMatrix), jointIndex); //	Note: bone->mOffsetMatrix already has skeletonTranform built into it
						//m_BoneInfo.emplace_back(Float4x4FromAIMatrix4x4(bone->mOffsetMatrix), jointIndex); //	Note: bone->mOffsetMatrix already has skeletonTranform built into it
					}

					for (size_t j = 0; j < bone->mNumWeights; j++)
					{
						int VertexID = submesh.BaseVertex + bone->mWeights[j].mVertexId;
						float Weight = bone->mWeights[j].mWeight;
						m_SkinnedVertices[VertexID].AddBoneData(boneIndex, Weight);
					}

				}

			}

			m_BoneTransforms.resize(m_BoneInfo.size());

			m_Animations.resize(scene->mNumAnimations);
			for (size_t m = 0; m < scene->mNumAnimations; m++)
			{
				const aiAnimation* animation = scene->mAnimations[m];
				m_Animations[m] = Ref<Animation>::Create(animation, this);
			}

			for (size_t i = 0; i < m_BoneInfo.size(); ++i)
				m_BoneTransforms[i] = glm::mat4(FLT_MAX);

		}




		{
			// Vertex/Index buffer
			if(m_IsAnimated)
				m_VertexBuffer = VertexBuffer::Create(m_SkinnedVertices.data(), m_SkinnedVertices.size() * sizeof(AnimatedVertex));
			else
				m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), m_Vertices.size() * sizeof(Vertex));

			m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size() * sizeof(Index));

			// Instanced vertex buffer
			uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;

			m_VertexBufferInstanced.resize(framesInFlight);
			m_VertexBufferInstanced_CPU.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_VertexBufferInstanced[i] = BufferDevice::Create(m_Submeshes.size() * sizeof(SubmeshInstanced), {BufferUsage::Vertex});
				m_VertexBufferInstanced_CPU[i].Allocate(m_Submeshes.size() * sizeof(SubmeshInstanced) + 1);
			}


			if (m_IsAnimated)
			{
				m_BoneTransformsUniformBuffer.resize(framesInFlight);
				for (uint32_t i = 0; i < framesInFlight; i++)
				{
					m_BoneTransformsUniformBuffer[i] = UniformBuffer::Create(sizeof(glm::mat4) * m_BoneTransforms.size());
				}
			}

			if (meshBuildSettings.CreateBottomLevelStructure)
			{
				// Acceleration structure creation
				MeshASInfo meshInfo{};
				meshInfo.MeshVertexBuffer = m_VertexBuffer;
				meshInfo.MeshIndexBuffer = m_IndexBuffer;
				meshInfo.SubmeshIndexBuffer = m_SubmeshIndexBuffers;
				meshInfo.SubMeshes = m_Submeshes;

				m_AccelerationStructure = BottomLevelAccelerationStructure::Create(meshInfo);
			}
		}

		// Materials
		if (scene->HasMaterials() && meshBuildSettings.LoadMaterials)
		{
			m_Textures.reserve(scene->mNumMaterials);
			m_TexturesFilepaths.resize(scene->mNumMaterials);
			//m_Materials.resize(scene->mNumMaterials);
			m_MaterialData.resize(scene->mNumMaterials);

			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
			{
				// Albedo -         vec4        (16 bytes)
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

				auto aiMaterial = scene->mMaterials[i];
				auto aiMaterialName = aiMaterial->GetName();

				//auto mi = Material::Create(m_MeshShader, aiMaterialName.data);
				//m_Materials[i] = mi;
				
				uint32_t textureCount = aiMaterial->GetTextureCount(aiTextureType_DIFFUSE);

				aiColor3D aiColor, aiEmission;
				// Getting the albedo color
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
				{
					glm::vec3 materialColor = { aiColor.r, aiColor.g, aiColor.b };

					//mi->Set("u_MaterialUniform.AlbedoColor", materialColor);
					m_MaterialData[i].Set("AlbedoColor", glm::vec4(materialColor, 1.0f));
				}

				// Geting the emission factor
				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS)
				{
					////mi->Set("u_MaterialUniform.Emission", aiEmission.r);
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

						uint32_t textureId = m_TextureAllocatorSlots[albedoTextureIndex];
						m_Textures[textureId] = texture;
						m_TexturesFilepaths[i].AlbedoFilepath = texturePath;
						
						VulkanBindlessAllocator::AddTextureCustomSlot(texture, textureId);
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);

						uint32_t textureId = m_TextureAllocatorSlots[albedoTextureIndex];
						m_Textures[textureId] = whiteTexture;

						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}
				}
				else
				{
					uint32_t textureId = m_TextureAllocatorSlots[albedoTextureIndex];
					m_Textures[textureId] = whiteTexture;
					
					VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
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
						uint32_t textureId = m_TextureAllocatorSlots[normalMapTextureIndex];
						m_Textures[textureId] = texture;
						m_TexturesFilepaths[i].NormalMapFilepath = texturePath;

						m_MaterialData[i].Set("UseNormalMap", uint32_t(1));
						VulkanBindlessAllocator::AddTextureCustomSlot(texture, textureId);
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load albedo texture: {0}", texturePath);
						
						uint32_t textureId = m_TextureAllocatorSlots[normalMapTextureIndex];
						m_Textures[textureId] = whiteTexture;

						m_MaterialData[i].Set("UseNormalMap", uint32_t(0));
						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}
				}
				else
				{
					uint32_t textureId = m_TextureAllocatorSlots[normalMapTextureIndex];
					m_Textures[textureId] = whiteTexture;

					m_MaterialData[i].Set("UseNormalMap", uint32_t(0));
					VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
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
						uint32_t textureId = m_TextureAllocatorSlots[roughnessTextureIndex];
						m_Textures[textureId] = texture;
						m_TexturesFilepaths[i].RoughnessMapFilepath = texturePath;

						VulkanBindlessAllocator::AddTextureCustomSlot(texture, textureId);
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load normal texture: {0}", texturePath);

						uint32_t textureId = m_TextureAllocatorSlots[roughnessTextureIndex];
						m_Textures[textureId] = whiteTexture;
						
						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}
				}
				else
				{
					uint32_t textureId = m_TextureAllocatorSlots[roughnessTextureIndex];
					m_Textures[textureId] = whiteTexture;
					
					VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
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

								uint32_t textureId = m_TextureAllocatorSlots[metalnessTextureIndex];
								m_Textures[textureId] = texture;
								m_TexturesFilepaths[i].MetalnessMapFilepath = texturePath;

								VulkanBindlessAllocator::AddTextureCustomSlot(texture, textureId);
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
						uint32_t textureId = m_TextureAllocatorSlots[metalnessTextureIndex];
						m_Textures[textureId] = whiteTexture;
						
						VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureId);
					}

				}
			}
		}
		else
		{
			//auto mi = Material::Create(m_MeshShader, "MeshShader-Default");
			//mi->Set("u_AlbedoTexture", whiteTexture);
			//mi->Set("u_NormalTexture", whiteTexture);
			//mi->Set("u_MetalnessTexture", whiteTexture);

			//mi->Set("u_MaterialUniform.AlbedoColor", glm::vec3(0.8f));
			//mi->Set("u_MaterialUniform.Emission", 0.0f);
			//mi->Set("u_MaterialUniform.Roughness", 0.0f);
			//mi->Set("u_MaterialUniform.Metalness", 0.0f);
			//mi->Set("u_MaterialUniform.UseNormalMap", uint32_t(0));
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
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			TraverseNodes(node->mChildren[i], transform, level + 1);
	}

	void Mesh::UpdateInstancedVertexBuffer(const glm::mat4& transform, const glm::mat4& viewProjMatrix, uint32_t currentFrameIndex)
	{
		// Instanced data for submeshes
		SubmeshInstanced submeshInstanced{};
		uint32_t vboInstancedDataOffset = 0;

		//for (uint32_t i = 0; i < m_Submeshes.size(); i++)
		for (auto& submesh : m_Submeshes)
		{
			glm::mat4 modelMatrix = transform * submesh.Transform;

			// Submit instanced data into a cpu buffer (which will be later sent to the gpu's instanced vbo)
			submeshInstanced.ModelSpaceMatrix = modelMatrix;
			submeshInstanced.WorldSpaceMatrix = viewProjMatrix * modelMatrix;

			m_VertexBufferInstanced_CPU[currentFrameIndex].Write((void*)&submeshInstanced, sizeof(SubmeshInstanced), vboInstancedDataOffset);

			vboInstancedDataOffset += sizeof(SubmeshInstanced);
		}

		// Submit instanced data from a cpu buffer to gpu vertex buffer
		auto vulkanVBOInstanced = m_VertexBufferInstanced[currentFrameIndex];
		vulkanVBOInstanced->SetData(vboInstancedDataOffset, m_VertexBufferInstanced_CPU[currentFrameIndex].Data);

		if (m_IsAnimated)
		{
			m_BoneTransformsUniformBuffer[currentFrameIndex]->SetData(m_BoneTransforms.data());
		}

	}

	static glm::mat4 Mat4FromFloat4x4(const ozz::math::Float4x4& float4x4)
	{
		glm::mat4 result;
		ozz::math::StorePtr(float4x4.cols[0], glm::value_ptr(result[0]));
		ozz::math::StorePtr(float4x4.cols[1], glm::value_ptr(result[1]));
		ozz::math::StorePtr(float4x4.cols[2], glm::value_ptr(result[2]));
		ozz::math::StorePtr(float4x4.cols[3], glm::value_ptr(result[3]));
		return result;
	}

	void Mesh::UpdateBoneTransformMatrices(const ozz::vector<ozz::math::Float4x4>& modelSpaceMatrices)
	{
		if (m_IsAnimated)
		{

			//m_AnimationController->OnUpdate(deltaTime);
			if (modelSpaceMatrices.empty() || modelSpaceMatrices.size() < m_BoneInfo.size())
			{
				for (size_t i = 0; i < m_BoneInfo.size(); ++i)
					m_BoneTransforms[i] = glm::mat4(FLT_MAX);
			}
			else
			{
				for (size_t i = 0; i < m_BoneInfo.size(); ++i)
				{
					uint32_t jointIndex = m_BoneInfo[i].JointIndex;
					m_BoneTransforms[i] = Mat4FromFloat4x4(modelSpaceMatrices[jointIndex]) * m_BoneInfo[i].InverseBindPose;
				}
			}
			//BoneTransform(deltaTime);
			//m_BoneTransformsUniformBuffer[currentFrameIndex]->SetData(m_BoneTransforms.data());
		}
	}

	void Mesh::SetNewTexture(uint32_t textureId, Ref<Texture2D> texture)
	{
		if (texture->Loaded())
		{
			if (m_Textures.find(textureId) != m_Textures.end())
			{
				VulkanBindlessAllocator::AddTextureCustomSlot(texture, textureId);
				m_Textures[textureId] = texture;
			}
		}
	}

	Mesh::~Mesh()
	{
		auto whiteTexture = Renderer::GetWhiteLUT();
		for (auto& textureSlotPair : m_TextureAllocatorSlots)
		{
			uint32_t textureSlot = textureSlotPair.second;
			VulkanBindlessAllocator::AddTextureCustomSlot(whiteTexture, textureSlot);
			VulkanBindlessAllocator::RemoveTextureCustomSlot(textureSlot);
		}
	}

}