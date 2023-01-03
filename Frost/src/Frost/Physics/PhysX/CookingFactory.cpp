#include "frostpch.h"
#include "CookingFactory.h"

#include "PhysXInternal.h"
#include "Frost/Utils/FileSystem.h"

namespace Frost
{
	struct CookingData
	{
		physx::PxCooking* CookingSDK;
		physx::PxCookingParams CookingParameters;

		CookingData(const physx::PxTolerancesScale& scale)
			: CookingSDK(nullptr), CookingParameters(scale)
		{
		}
	};

	static CookingData* s_CookingData = nullptr;
	void CookingFactory::Initialize()
	{
		s_CookingData = new CookingData(PhysXInternal::GetPhysXHandle().getTolerancesScale());
		s_CookingData->CookingParameters.meshWeldTolerance = 0.1f;
		s_CookingData->CookingParameters.meshPreprocessParams = physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
		s_CookingData->CookingParameters.midphaseDesc = physx::PxMeshMidPhase::eBVH34;

		s_CookingData->CookingSDK = PxCreateCooking(PX_PHYSICS_VERSION, PhysXInternal::GetFoundation(), s_CookingData->CookingParameters);
		FROST_ASSERT(s_CookingData->CookingSDK, "Couldn't initialize PhysX Cooking SDK!");
	}

	void CookingFactory::Shutdown()
	{
		s_CookingData->CookingSDK->release();
		s_CookingData->CookingSDK = nullptr;

		delete s_CookingData;
	}

	namespace Utils
	{
		static std::filesystem::path GetCacheDirectory()
		{
			return std::filesystem::path("Resources/Cache/Colliders");
		}

		static void CreateCacheDirectoryIfNeeded()
		{
			std::filesystem::path cacheDirectory = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}
	}

	CookingResult CookingFactory::CookMesh(MeshColliderComponent& component, bool invalidateOld, Vector<MeshColliderData>& outData)
	{
		const auto& mesh = component.CollisionMesh;

		Utils::CreateCacheDirectoryIfNeeded();


		std::string meshName = std::filesystem::path(mesh->GetFilepath()).stem().string();
		std::filesystem::path filepath = Utils::GetCacheDirectory() / (meshName + (component.IsConvex ? "_convex.fpo" : "_tri.fpo")); // Frost Physics Object

		CookingResult result = CookingResult::Failure;
		if (!std::filesystem::exists(filepath))
		{
			result = component.IsConvex ? CookConvexMesh(mesh, outData) : CookTriangleMesh(mesh, outData);

			if (result == CookingResult::Success)
			{
				// Serialize Collider Data
				uint32_t bufferSize = 0;
				uint32_t offset = 0;

				for (auto& colliderData : outData)
				{
					bufferSize += sizeof(uint32_t);
					bufferSize += colliderData.Size;
				}

				Buffer colliderBuffer;
				colliderBuffer.Allocate(bufferSize);

				for (auto& colliderData : outData)
				{
					colliderBuffer.Write((void*)&colliderData.Size, sizeof(uint32_t), offset);
					offset += sizeof(uint32_t);
					colliderBuffer.Write(colliderData.Data, colliderData.Size, offset);
					offset += colliderData.Size;
				}

				bool success = FileSystem::WriteBytes(filepath, colliderBuffer);
				colliderBuffer.Release();

				if (!success)
				{
					FROST_CORE_ERROR("Failed to write collider to {0}", filepath.string());
					return CookingResult::Failure;
				}
			}
		}
		else
		{
#if 1
			Buffer colliderBuffer = FileSystem::ReadBytes(filepath);

			if (colliderBuffer.Size > 0)
			{
				uint32_t offset = 0;

				const auto& submeshes = mesh->GetSubMeshes();

				for (int32_t submeshIndex = 0; submeshIndex < mesh->GetSubMeshes().size(); submeshIndex++)
				{
					MeshColliderData& data = outData.emplace_back();

					data.Size = colliderBuffer.Read<uint32_t>(offset);
					offset += sizeof(uint32_t);
					data.Data = colliderBuffer.ReadBytes(data.Size, offset);
					offset += data.Size;
					data.Transform = submeshes[submeshIndex].Transform;
				}

				colliderBuffer.Release();

				result = CookingResult::Success;
			}
#endif
		}

#if 0
		// TODO: Generate debug mesh!
		if (result == CookingResult::Success && component.ProcessedMeshes.size() == 0)
		{
			for (const auto& colliderData : outData)
				GenerateDebugMesh(component, colliderData);
		}
#endif

		return result;
	}

	CookingResult CookingFactory::CookConvexMesh(const Ref<Mesh>& mesh, Vector<MeshColliderData>& outdata)
	{
		const auto& vertices = mesh->GetVertices();
		const auto& indices = mesh->GetIndices();
		const auto& submeshes = mesh->GetSubMeshes();

		for (int32_t submeshIndex = 0; submeshIndex < mesh->GetSubMeshes().size(); submeshIndex++)
		{
			const auto& submesh = submeshes[submeshIndex];

			physx::PxConvexMeshDesc convexDesc;
			convexDesc.points.count = submesh.VertexCount;
			convexDesc.points.stride = sizeof(Vertex);
			convexDesc.points.data = &vertices[submesh.BaseVertex];
			convexDesc.indices.count = submesh.IndexCount / 3;
			convexDesc.indices.data = &indices[submesh.BaseIndex / 3];
			convexDesc.indices.stride = sizeof(Index);
			convexDesc.flags = physx::PxConvexFlag::eCOMPUTE_CONVEX | physx::PxConvexFlag::eSHIFT_VERTICES;

			physx::PxDefaultMemoryOutputStream buf;
			physx::PxConvexMeshCookingResult::Enum result;
			if (!s_CookingData->CookingSDK->cookConvexMesh(convexDesc, buf, &result))
			{
				FROST_CORE_ERROR("Failed to cook convex mesh {0}", submesh.MeshName);
				outdata.clear();
				return PhysXUtils::FromPhysXCookingResult(result);
			}

			MeshColliderData& data = outdata.emplace_back();
			data.Size = buf.getSize();
			data.Data = new Byte[data.Size];
			data.Transform = submesh.Transform;
			memcpy(data.Data, buf.getData(), data.Size);
		}

		return CookingResult::Success;
	}

	CookingResult CookingFactory::CookTriangleMesh(const Ref<Mesh>& mesh, Vector<MeshColliderData>& outdata)
	{
		const auto& vertices = mesh->GetVertices();
		const auto& indices = mesh->GetIndices();
		const auto& submeshes = mesh->GetSubMeshes();

		for (int32_t submeshIndex = 0; submeshIndex < mesh->GetSubMeshes().size(); submeshIndex++)
		{
			const auto& submesh = submeshes[submeshIndex];

			physx::PxTriangleMeshDesc triangleDesc;
			triangleDesc.points.count = submesh.VertexCount;
			triangleDesc.points.stride = sizeof(Vertex);
			triangleDesc.points.data = &vertices[submesh.BaseVertex];
			triangleDesc.triangles.count = submesh.IndexCount / 3;
			triangleDesc.triangles.data = &indices[submesh.BaseIndex / 3];
			triangleDesc.triangles.stride = sizeof(Index);

			physx::PxDefaultMemoryOutputStream buf;
			physx::PxTriangleMeshCookingResult::Enum result;
			if (!s_CookingData->CookingSDK->cookTriangleMesh(triangleDesc, buf, &result))
			{
				FROST_CORE_ERROR("Failed to cook convex mesh {0}", submesh.MeshName);
				outdata.clear();
				return PhysXUtils::FromPhysXCookingResult(result);
			}

			MeshColliderData& data = outdata.emplace_back();
			data.Size = buf.getSize();
			data.Data = new Byte[data.Size];
			data.Transform = submesh.Transform;
			memcpy(data.Data, buf.getData(), data.Size);
		}

		return CookingResult::Success;
	}

	void CookingFactory::GenerateDebugMesh(MeshColliderComponent& component, const MeshColliderData& colliderData)
	{
		physx::PxDefaultMemoryInputData input(colliderData.Data, colliderData.Size);

		std::vector<Vertex> vertices;
		std::vector<Index> indices;

		if (component.IsConvex)
		{
			physx::PxConvexMesh* convexMesh = PhysXInternal::GetPhysXHandle().createConvexMesh(input);

			// Magic values since we can't easily extract the vertex/index count from convexMesh
			vertices.reserve(100);
			indices.reserve(50);

			// Based On: https://github.com/EpicGames/UnrealEngine/blob/release/Engine/Source/ThirdParty/PhysX3/NvCloth/samples/SampleBase/renderer/ConvexRenderMesh.cpp
			const uint32_t nbPolygons = convexMesh->getNbPolygons();
			const physx::PxVec3* convexVertices = convexMesh->getVertices();
			const physx::PxU8* convexIndices = convexMesh->getIndexBuffer();

			uint32_t nbVertices = 0;
			uint32_t nbFaces = 0;
			uint32_t vertCounter = 0;
			uint32_t indexCounter = 0;

			for (uint32_t i = 0; i < nbPolygons; i++)
			{
				physx::PxHullPolygon polygon;
				convexMesh->getPolygonData(i, polygon);
				nbVertices += polygon.mNbVerts;
				nbFaces += (polygon.mNbVerts - 2) * 3;

				uint32_t vI0 = vertCounter;
				for (uint32_t vI = 0; vI < polygon.mNbVerts; vI++)
				{
					Vertex& v = vertices.emplace_back();
					v.Position = PhysXUtils::FromPhysXVector(convexVertices[convexIndices[polygon.mIndexBase + vI]]);
					vertCounter++;
				}

				for (uint32_t vI = 1; vI < uint32_t(polygon.mNbVerts) - 1; vI++)
				{
					Index& index = indices.emplace_back();
					index.V1 = uint32_t(vI0);
					index.V2 = uint32_t(vI0 + vI + 1);
					index.V3 = uint32_t(vI0 + vI);
					indexCounter++;
				}
			}

			convexMesh->release();
		}
		else
		{
			physx::PxTriangleMesh* trimesh = PhysXInternal::GetPhysXHandle().createTriangleMesh(input);

			const uint32_t nbVerts = trimesh->getNbVertices();
			const physx::PxVec3* triangleVertices = trimesh->getVertices();
			const uint32_t nbTriangles = trimesh->getNbTriangles();
			const physx::PxU16* tris = (const physx::PxU16*)trimesh->getTriangles();

			vertices.reserve(nbVerts);
			indices.reserve(nbTriangles);

			for (uint32_t v = 0; v < nbVerts; v++)
			{
				Vertex& v1 = vertices.emplace_back();
				v1.Position = PhysXUtils::FromPhysXVector(triangleVertices[v]);
			}

			for (uint32_t tri = 0; tri < nbTriangles; tri++)
			{
				Index& index = indices.emplace_back();
				index.V1 = tris[3 * tri + 0];
				index.V2 = tris[3 * tri + 1];
				index.V3 = tris[3 * tri + 2];
			}

			trimesh->release();
		}

		//Ref<MeshAsset> meshAsset = Ref<MeshAsset>::Create(vertices, indices, colliderData.Transform);
		//component.ProcessedMeshes.push_back(Ref<Mesh>::Create(meshAsset));
	}

}