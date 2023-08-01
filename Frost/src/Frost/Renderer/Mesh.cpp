#include "frostpch.h"
#include "Mesh.h"

#include "Frost/Utils/Timer.h"
#include "Frost/Asset/AssetManager.h"

#include "Frost/Renderer/Renderer.h"
#include "Frost/Renderer/Animation.h"
#include "Frost/Renderer/OZZAssimpImporter.h"
#include "Frost/Renderer/BindlessAllocator.h"

#include "Frost/EntitySystem/Scene.h"
#include "Frost/EntitySystem/Entity.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/base/maths/simd_math.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#define MAX_BONES 400

namespace Frost
{
	static DefaultMeshStorage s_DefaultMeshStorage;

	Ref<MeshAsset> MeshAsset::Load(const std::string& filepath, MaterialInstance material /*= {}*/)
	{
		return CreateRef<MeshAsset>(filepath, material);
	}

	Ref<MeshAsset> MeshAsset::LoadCustomMesh(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings /*= {}*/)
	{
		return CreateRef<MeshAsset>(filepath, material, meshBuildSettings);
	}

	void MeshAsset::InitDefaultMeshes()
	{
		s_DefaultMeshStorage.Cube = MeshAsset::Load("Resources/Meshes/.Default/Cube.fbx");
		s_DefaultMeshStorage.Sphere = MeshAsset::Load("Resources/Meshes/.Default/Sphere.fbx");
		s_DefaultMeshStorage.Capsule = MeshAsset::Load("Resources/Meshes/.Default/Capsule.fbx");
	}

	const DefaultMeshStorage& MeshAsset::GetDefaultMeshes()
	{
		return s_DefaultMeshStorage;
	}

	void MeshAsset::DestroyDefaultMeshes()
	{
		//s_DefaultMeshStorage.Cube.~Ref();
		s_DefaultMeshStorage.Cube = nullptr;

		//s_DefaultMeshStorage.Sphere.~Ref();
		s_DefaultMeshStorage.Sphere = nullptr;

		//s_DefaultMeshStorage.Capsule.~Ref();
		s_DefaultMeshStorage.Capsule = nullptr;
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

	MeshAsset::MeshAsset(const std::string& filepath, MaterialInstance material, MeshBuildSettings meshBuildSettings)
		: m_Material(material), m_Filepath(filepath)
	{
		m_Importer = CreateScope<Assimp::Importer>();

		const aiScene* scene = m_Importer->ReadFile(filepath, Utils::s_MeshImportFlags);

		if ((!scene || !scene->HasMeshes()))
		{
			FROST_CORE_ERROR(m_Importer->GetErrorString());
			return;
		}
		FROST_ASSERT(!(!scene || !scene->HasMeshes()), m_Importer->GetErrorString());

		m_Scene = scene;
		m_IsLoaded = true;
		m_IsAnimated = scene->mAnimations != nullptr;

		if (m_IsAnimated)
		{
			m_Skeleton = CreateRef<MeshSkeleton>(scene);
		}



		Ref<Texture2D> whiteTexture = Renderer::GetWhiteLUT();
#if 0
		// Allocate texture slots before storing the vertex data, because we are using bindless
		// We are using `scene->mNumMaterials * 4`, because each mesh has a albedo, roughness, metalness and normal map
		for (uint32_t i = 0; i < scene->mNumMaterials * 4; i++)
		{
			uint32_t textureSlot = BindlessAllocator::AddTexture(whiteTexture);
			m_TextureAllocatorSlots[i] = textureSlot;
		}
#endif


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
			submesh.BaseIndex = indexCount;				  // Indices information
			submesh.VertexCount = mesh->mNumVertices;     // Vertices information
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

			m_Animations.resize(scene->mNumAnimations);
			for (size_t m = 0; m < scene->mNumAnimations; m++)
			{
				const aiAnimation* animation = scene->mAnimations[m];
				m_Animations[m] = Ref<Animation>::Create(animation, this);
			}
		}




		// Vertex/Index buffer
		if (m_IsAnimated)
			m_VertexBuffer = VertexBuffer::Create(m_SkinnedVertices.data(), m_SkinnedVertices.size() * sizeof(AnimatedVertex));
		else
			m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), m_Vertices.size() * sizeof(Vertex));

		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)m_Indices.size() * sizeof(Index));

		// Acceleration structure (for Ray Tracing)
		if (meshBuildSettings.CreateBottomLevelStructure)
		{
			// Acceleration structure creation
			MeshASInfo meshInfo{};
			meshInfo.MeshVertexBuffer = m_VertexBuffer;
			meshInfo.MeshIndexBuffer = m_IndexBuffer;
			meshInfo.SubmeshIndexBuffer = m_SubmeshIndexBuffers;
			meshInfo.SubMeshes = m_Submeshes;

#if FROST_SUPPORT_RAY_TRACING
			m_AccelerationStructure = BottomLevelAccelerationStructure::Create(meshInfo);
#endif
		}

		// Materials
		if (scene->HasMaterials() && meshBuildSettings.LoadMaterials)
		{
			//m_Textures.reserve(scene->mNumMaterials);
			m_TexturesList.resize(scene->mNumMaterials * 4);
			m_MaterialData.resize(scene->mNumMaterials);
			m_MaterialNames.resize(scene->mNumMaterials);

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
				materialData.Add("AlbedoColor", glm::vec4(0.0f));
				materialData.Add("EmissionFactor", 0.0f);
				materialData.Add("RoughnessFactor", 0.0f);
				materialData.Add("MetalnessFactor", 0.0f);

				materialData.Add("UseNormalMap", 0);

				materialData.Add("AlbedoTexture", 0);
				materialData.Add("RoughnessTexture", 0);
				materialData.Add("MetalnessTexture", 0);
				materialData.Add("NormalTexture", 0);


				// Each mesh has 4 textures, and se we allocated numMaterials * 4 texture slots.
				uint32_t albedoTextureIndex = (i * 4) + 0;
				uint32_t roughnessTextureIndex = (i * 4) + 1;
				uint32_t metalnessTextureIndex = (i * 4) + 2;
				uint32_t normalMapTextureIndex = (i * 4) + 3;


				//m_MaterialData[i].Set("AlbedoTexture", m_TextureAllocatorSlots[albedoTextureIndex]);
				//m_MaterialData[i].Set("NormalTexture", m_TextureAllocatorSlots[normalMapTextureIndex]);
				//m_MaterialData[i].Set("RoughnessTexture", m_TextureAllocatorSlots[roughnessTextureIndex]);
				//m_MaterialData[i].Set("MetalnessTexture", m_TextureAllocatorSlots[metalnessTextureIndex]);

				auto aiMaterial = scene->mMaterials[i];

				auto aiMaterialName = aiMaterial->GetName();
				m_MaterialNames[i] = aiMaterialName.C_Str();

				uint32_t textureCount = aiMaterial->GetTextureCount(aiTextureType_DIFFUSE);

				aiColor3D aiColor, aiEmission;
				// Getting the albedo color
				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
				{
					glm::vec3 materialColor = { aiColor.r, aiColor.g, aiColor.b };

					m_MaterialData[i].Set("AlbedoColor", glm::vec4(materialColor, 1.0f));
				}

				// Geting the emission factor
				if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, aiEmission) == AI_SUCCESS)
				{
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
					std::replace(texturePath.begin(), texturePath.end(), '/', '\\'); // This is the standart rule of paths in this engine

					TextureSpecification textureSpec{};
					textureSpec.Format = ImageFormat::RGBA8;
					textureSpec.Usage = ImageUsage::ReadOnly;
					textureSpec.UseMips = true;
					textureSpec.FlipTexture = true;
					Ref<Texture2D> texture = AssetManager::GetOrLoadAsset<Texture2D>(texturePath, (void*)&textureSpec);
					if (texture->Loaded())
					{
						texture->GenerateMipMaps();
						m_TexturesList[albedoTextureIndex] = texture;
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load texture: {0}", texturePath);
						m_TexturesList[albedoTextureIndex] = whiteTexture;
					}
				}
				else
				{
					m_TexturesList[albedoTextureIndex] = whiteTexture;
				}


				// Normal map
				bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiTexPath) == AI_SUCCESS;
				if (hasNormalMap)
				{
					std::filesystem::path path = filepath;
					auto parentPath = path.parent_path();
					parentPath /= std::string(aiTexPath.data);
					std::string texturePath = parentPath.string();
					std::replace(texturePath.begin(), texturePath.end(), '/', '\\'); // This is the standart rule of paths in this engine

					TextureSpecification textureSpec{};
					textureSpec.Format = ImageFormat::RGBA8;
					textureSpec.Usage = ImageUsage::ReadOnly;
					textureSpec.FlipTexture = true;
					Ref<Texture2D> texture = AssetManager::GetOrLoadAsset<Texture2D>(texturePath, (void*)&textureSpec);
					if (texture->Loaded())
					{
						m_TexturesList[normalMapTextureIndex] = texture;
						m_MaterialData[i].Set("UseNormalMap", uint32_t(1));
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load normal texture: {0}", texturePath);
						m_TexturesList[normalMapTextureIndex] = whiteTexture;
						m_MaterialData[i].Set("UseNormalMap", uint32_t(0));
					}
				}
				else
				{
					m_TexturesList[normalMapTextureIndex] = whiteTexture;
					m_MaterialData[i].Set("UseNormalMap", uint32_t(0));
				}


				// Roughness map
				bool hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
				if (hasRoughnessMap)
				{
					std::filesystem::path path = filepath;
					auto parentPath = path.parent_path();
					parentPath /= std::string(aiTexPath.data);
					std::string texturePath = parentPath.string();
					std::replace(texturePath.begin(), texturePath.end(), '/', '\\'); // This is the standart rule of paths in this engine

					TextureSpecification textureSpec{};
					textureSpec.Format = ImageFormat::RGBA8;
					textureSpec.Usage = ImageUsage::ReadOnly;
					textureSpec.FlipTexture = true;
					Ref<Texture2D> texture = AssetManager::GetOrLoadAsset<Texture2D>(texturePath, (void*)&textureSpec);
					if (texture->Loaded())
					{
						m_TexturesList[roughnessTextureIndex] = texture;
					}
					else
					{
						FROST_CORE_ERROR("Couldn't load roughess texture: {0}", texturePath);
						m_TexturesList[roughnessTextureIndex] = whiteTexture;
					}
				}
				else
				{
					m_TexturesList[roughnessTextureIndex] = whiteTexture;
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
							std::replace(texturePath.begin(), texturePath.end(), '/', '\\'); // This is the standart rule of paths in this engine

							TextureSpecification textureSpec{};
							textureSpec.Format = ImageFormat::RGBA8;
							textureSpec.Usage = ImageUsage::ReadOnly;
							textureSpec.FlipTexture = true;
							Ref<Texture2D> texture = AssetManager::GetOrLoadAsset<Texture2D>(texturePath, (void*)&textureSpec);
							if (texture->Loaded())
							{
								metalnessTextureFound = true;

								m_TexturesList[metalnessTextureIndex] = whiteTexture;
							}
							else
							{
								FROST_CORE_ERROR("Couldn't load metalness texture: {0}", texturePath);
							}
							break;
						}
					}

				}

				if (!metalnessTextureFound)
				{
					m_TexturesList[metalnessTextureIndex] = whiteTexture;
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

	MeshAsset::MeshAsset(const Vector<Vertex>& vertices, const Vector<Index>& indices, const glm::mat4& transform)
	{
		Submesh submesh;
		submesh.BaseVertex = 0;
		submesh.BaseIndex = 0;
		submesh.IndexCount = (uint32_t)indices.size() * 3u;
		submesh.Transform = transform;
		m_Submeshes.push_back(submesh);

		m_Vertices = vertices;
		m_Indices = indices;

		m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), (uint32_t)(m_Vertices.size() * sizeof(Vertex)));
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), (uint32_t)(m_Indices.size() * sizeof(Index)));

		m_IsLoaded = true;
	}

	bool MeshAsset::ReloadData(const std::string& filepath)
	{
		std::string totalFilepath = AssetManager::GetFileSystemPathString(AssetManager::GetMetadata(filepath));
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		size_t numMaterials = m_MaterialData.size();

		Ref<MeshAsset> newMeshAsset = MeshAsset::Load(totalFilepath);

		if (!newMeshAsset->IsLoaded())
		{
			FROST_CORE_ERROR("Reloaded Mesh Asset is invalid!");
			return false;
		}

		// When reloading the mesh data, there might be a chance that the user wants to change the materials or submeshes or even bone information.
		// In that case we should retrieve all the meshes from the AssetManager which use this MeshAsset and reset their specificed paramaters to be updated to MeshAsset
		bool doAllMeshesNeedReset = false;
		if (newMeshAsset->m_Submeshes.size() != m_Submeshes.size() ||
			newMeshAsset->m_MaterialData.size() != m_MaterialData.size() ||
			newMeshAsset->m_BoneInfo.size() != m_BoneInfo.size())
		{
			doAllMeshesNeedReset = true;
		}

		m_IsLoaded = newMeshAsset->m_IsLoaded;
		m_IsAnimated = newMeshAsset->m_IsAnimated;

		// Mesh data
		m_Vertices = newMeshAsset->m_Vertices;
		m_SkinnedVertices = newMeshAsset->m_SkinnedVertices;
		m_Indices = newMeshAsset->m_Indices;
		m_Submeshes = newMeshAsset->m_Submeshes;

		// Bone/Animation information
		m_BoneCount = newMeshAsset->m_BoneCount;
		m_BoneInfo = newMeshAsset->m_BoneInfo;
		m_Skeleton = newMeshAsset->m_Skeleton;
		m_Animations = newMeshAsset->m_Animations;

		m_VertexBuffer = newMeshAsset->m_VertexBuffer;
		m_IndexBuffer = newMeshAsset->m_IndexBuffer;

		m_TexturesList = newMeshAsset->m_TexturesList;
		m_MaterialData = newMeshAsset->m_MaterialData;
		m_MaterialNames = newMeshAsset->m_MaterialNames;

		m_AccelerationStructure = newMeshAsset->m_AccelerationStructure;
		m_SubmeshIndexBuffers = newMeshAsset->m_SubmeshIndexBuffers;

		// There is an explanation above for why we need this
		if (doAllMeshesNeedReset)
		{
			//auto meshes = AssetManager::GetAllLoadedAssetsByType<Mesh>();
			Ref<Scene> activeRenderingScene = Renderer::GetActiveScene();
			auto entitiesWithMeshComponent = activeRenderingScene->GetAllEntitiesWith<MeshComponent>();


			for (auto& entity : entitiesWithMeshComponent)
			{
				Entity e = Entity(entity, activeRenderingScene.Raw());
				MeshComponent& meshComponent = e.GetComponent<MeshComponent>();

				if (meshComponent.IsMeshValid())
				{
					Ref<Mesh> mesh = meshComponent.Mesh;

					if (mesh->GetMeshAsset()->GetFilepath() == m_Filepath)
					{
						mesh->m_MaterialAssets.resize(numMaterials);
						for (uint32_t i = 0; i < numMaterials; i++)
						{
							mesh->m_MaterialAssets[i] = Ref<MaterialAsset>::Create();
							Ref<MaterialAsset> materialAsset = mesh->m_MaterialAssets[i];

							std::string materialName = m_MaterialNames[i];
							if (!materialName.empty())
								materialAsset->SetMaterialName(m_MaterialNames[i]);
							else
								materialAsset->SetMaterialName("Default");

							materialAsset->SetAlbedoColor(m_MaterialData[i].Get<glm::vec4>("AlbedoColor"));
							materialAsset->SetEmission(m_MaterialData[i].Get<float>("EmissionFactor"));
							materialAsset->SetRoughness(m_MaterialData[i].Get<float>("RoughnessFactor"));
							materialAsset->SetMetalness(m_MaterialData[i].Get<float>("MetalnessFactor"));
							materialAsset->SetUseNormalMap(m_MaterialData[i].Get<uint32_t>("UseNormalMap"));

							// Each mesh has 4 textures, and se we allocated numMaterials * 4 texture slots.
							uint32_t albedoTextureIndex = (i * 4) + 0;
							uint32_t roughnessTextureIndex = (i * 4) + 1;
							uint32_t metalnessTextureIndex = (i * 4) + 2;
							uint32_t normalMapTextureIndex = (i * 4) + 3;

							materialAsset->SetAlbedoMap(m_TexturesList[albedoTextureIndex]);
							materialAsset->SetRoughnessMap(m_TexturesList[roughnessTextureIndex]);
							materialAsset->SetMetalnessMap(m_TexturesList[metalnessTextureIndex]);
							materialAsset->SetNormalMap(m_TexturesList[normalMapTextureIndex]);
						}

						m_Submeshes.resize(m_Submeshes.size());
						for (uint32_t submeshIndex = 0; submeshIndex < m_Submeshes.size(); submeshIndex++)
						{
							const auto& submesh = m_Submeshes[submeshIndex];
							m_Submeshes[submeshIndex].Transform = submesh.Transform;
						}

						// Setting up the animations for the new Mesh, using information from the Mesh Asset
						if (m_IsAnimated)
						{
							mesh->m_BoneTransforms.resize(m_BoneInfo.size());
							for (size_t i = 0; i < m_BoneInfo.size(); ++i)
								mesh->m_BoneTransforms[i] = glm::mat4(FLT_MAX);

							mesh->m_BoneTransformsUniformBuffer.resize(framesInFlight);
							for (uint32_t i = 0; i < framesInFlight; i++)
								mesh->m_BoneTransformsUniformBuffer[i] = UniformBuffer::Create(sizeof(glm::mat4) * mesh->m_BoneTransforms.size());
						}

					}
				}
			}
		}


#if 0
		void* memory = this;
		AssetHandle previousAssetHandle = Handle;
		uint16_t previousAssetFlags = Flags;

		new(memory) MeshAsset(totalFilepath, MaterialInstance(), MeshBuildSettings());
		this->Handle = previousAssetHandle;
		this->Flags = previousAssetFlags;
#endif

		return true;
	}

	void MeshAsset::TraverseNodes(aiNode* node, const glm::mat4& parentTransform, uint32_t level)
	{
		glm::mat4 transform = parentTransform * Utils::AssimpMat4ToGlmMat4(node->mTransformation);
		for (uint32_t i = 0; i < node->mNumMeshes; i++)
		{
			uint32_t mesh = node->mMeshes[i];
			auto& submesh = m_Submeshes[mesh];
			submesh.MeshName = node->mName.C_Str();
			submesh.Transform = transform;
		}

		for (uint32_t i = 0; i < node->mNumChildren; i++)
			TraverseNodes(node->mChildren[i], transform, level + 1);
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

	MeshAsset::~MeshAsset()
	{
	}

	Mesh::Mesh(Ref<MeshAsset> meshAsset)
		: m_MeshAsset(meshAsset)
	{
		uint32_t framesInFlight = Renderer::GetRendererConfig().FramesInFlight;
		size_t numMaterials = m_MeshAsset->m_MaterialData.size();


		// Setting up the materials for the new Mesh, using information from the Mesh Asset
		// Materials
		m_MaterialAssets.resize(numMaterials);
		for (uint32_t i = 0; i < numMaterials; i++)
		{
			m_MaterialAssets[i] = Ref<MaterialAsset>::Create();
			Ref<MaterialAsset> materialAsset = m_MaterialAssets[i];

			std::string materialName = m_MeshAsset->m_MaterialNames[i];
			if (!materialName.empty())
				materialAsset->SetMaterialName(m_MeshAsset->m_MaterialNames[i]);
			else
				materialAsset->SetMaterialName("Default");

			materialAsset->SetAlbedoColor(m_MeshAsset->m_MaterialData[i].Get<glm::vec4>("AlbedoColor"));
			materialAsset->SetEmission(m_MeshAsset->m_MaterialData[i].Get<float>("EmissionFactor"));
			materialAsset->SetRoughness(m_MeshAsset->m_MaterialData[i].Get<float>("RoughnessFactor"));
			materialAsset->SetMetalness(m_MeshAsset->m_MaterialData[i].Get<float>("MetalnessFactor"));
			materialAsset->SetUseNormalMap(m_MeshAsset->m_MaterialData[i].Get<uint32_t>("UseNormalMap"));

			// Each mesh has 4 textures, and se we allocated numMaterials * 4 texture slots.
			uint32_t albedoTextureIndex = (i * 4) + 0;
			uint32_t roughnessTextureIndex = (i * 4) + 1;
			uint32_t metalnessTextureIndex = (i * 4) + 2;
			uint32_t normalMapTextureIndex = (i * 4) + 3;

			materialAsset->SetAlbedoMap(m_MeshAsset->m_TexturesList[albedoTextureIndex]);
			materialAsset->SetRoughnessMap(m_MeshAsset->m_TexturesList[roughnessTextureIndex]);
			materialAsset->SetMetalnessMap(m_MeshAsset->m_TexturesList[metalnessTextureIndex]);
			materialAsset->SetNormalMap(m_MeshAsset->m_TexturesList[normalMapTextureIndex]);
		}

		m_Submeshes.resize(m_MeshAsset->GetSubMeshes().size());
		for (uint32_t submeshIndex = 0; submeshIndex < m_MeshAsset->GetSubMeshes().size(); submeshIndex++)
		{
			const auto& submesh = m_MeshAsset->GetSubMeshes()[submeshIndex];
			m_Submeshes[submeshIndex].Transform = submesh.Transform;
		}

		// Setting up the animations for the new Mesh, using information from the Mesh Asset
		if (m_MeshAsset->IsAnimated())
		{
			m_BoneTransforms.resize(m_MeshAsset->m_BoneInfo.size());
			for (size_t i = 0; i < m_MeshAsset->m_BoneInfo.size(); ++i)
				m_BoneTransforms[i] = glm::mat4(FLT_MAX);

			m_BoneTransformsUniformBuffer.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++)
			{
				m_BoneTransformsUniformBuffer[i] = UniformBuffer::Create(sizeof(glm::mat4) * m_BoneTransforms.size());
			}
		}


		m_VertexBufferInstanced.resize(framesInFlight);
		m_VertexBufferInstanced_CPU.resize(framesInFlight);
		for (uint32_t i = 0; i < framesInFlight; i++)
		{
			uint32_t submeshCount = m_MeshAsset->m_Submeshes.size();

			m_VertexBufferInstanced[i] = BufferDevice::Create(
				submeshCount * sizeof(SubmeshInstanced),
				{ BufferUsage::Vertex }
			);

			m_VertexBufferInstanced_CPU[i].Allocate(submeshCount * sizeof(SubmeshInstanced));
		}
	}

	void Mesh::SetMaterialByAsset(uint32_t index, Ref<MaterialAsset> materialAsset)
	{
		m_MaterialAssets[index] = materialAsset;
	}

	void Mesh::SetMaterialAssetToDefault(uint32_t materialIndex)
	{
		m_MaterialAssets[materialIndex] = Ref<MaterialAsset>::Create();
		Ref<MaterialAsset> materialAsset = m_MaterialAssets[materialIndex];

		materialAsset->SetAlbedoColor(m_MeshAsset->m_MaterialData[materialIndex].Get<glm::vec4>("AlbedoColor"));
		materialAsset->SetEmission(m_MeshAsset->m_MaterialData[materialIndex].Get<float>("EmissionFactor"));
		materialAsset->SetRoughness(m_MeshAsset->m_MaterialData[materialIndex].Get<float>("RoughnessFactor"));
		materialAsset->SetMetalness(m_MeshAsset->m_MaterialData[materialIndex].Get<float>("MetalnessFactor"));
		materialAsset->SetUseNormalMap(m_MeshAsset->m_MaterialData[materialIndex].Get<uint32_t>("UseNormalMap"));

		// Each mesh has 4 textures, and se we allocated numMaterials * 4 texture slots.
		uint32_t albedoTextureIndex = (materialIndex * 4) + 0;
		uint32_t roughnessTextureIndex = (materialIndex * 4) + 1;
		uint32_t metalnessTextureIndex = (materialIndex * 4) + 2;
		uint32_t normalMapTextureIndex = (materialIndex * 4) + 3;

		materialAsset->SetAlbedoMap(m_MeshAsset->m_TexturesList[albedoTextureIndex]);
		materialAsset->SetRoughnessMap(m_MeshAsset->m_TexturesList[roughnessTextureIndex]);
		materialAsset->SetMetalnessMap(m_MeshAsset->m_TexturesList[metalnessTextureIndex]);
		materialAsset->SetNormalMap(m_MeshAsset->m_TexturesList[normalMapTextureIndex]);

	}

	void Mesh::SetNewTexture(uint32_t materialIndex, uint32_t textureId, Ref<Texture2D> texture)
	{
		m_MaterialAssets[materialIndex]->SetTextureById(textureId, texture);
	}

	void Mesh::UpdateBoneTransformMatrices(const ozz::vector<ozz::math::Float4x4>& modelSpaceMatrices)
	{
		uint32_t currentFrameIndex = Renderer::GetCurrentFrameIndex();

		if (m_MeshAsset->IsAnimated())
		{
			if (modelSpaceMatrices.empty() || modelSpaceMatrices.size() < m_MeshAsset->m_BoneInfo.size())
			{
				for (size_t i = 0; i < m_MeshAsset->m_BoneInfo.size(); ++i)
					m_BoneTransforms[i] = glm::mat4(FLT_MAX);
			}
			else
			{
				for (size_t i = 0; i < m_MeshAsset->m_BoneInfo.size(); ++i)
				{
					uint32_t jointIndex = m_MeshAsset->m_BoneInfo[i].JointIndex;
					m_BoneTransforms[i] = Mat4FromFloat4x4(modelSpaceMatrices[jointIndex]) * m_MeshAsset->m_BoneInfo[i].InverseBindPose;
				}
			}
		}

		if (m_MeshAsset->IsAnimated())
		{
			m_BoneTransformsUniformBuffer[currentFrameIndex]->SetData(m_BoneTransforms.data());
		}
	}

	Mesh::~Mesh()
	{
	}

}