#pragma once

#include "Frost/Core/Engine.h"
#include "Frost/Renderer/Mesh.h"
#include "PhysXUtils.h"

#include <PhysX/PxPhysicsAPI.h>

namespace Frost
{
	struct MeshColliderData
	{
		Byte* Data;
		glm::mat4 Transform;
		uint32_t Size;
	};

	class CookingFactory
	{
	public:
		static void Initialize();
		static void Shutdown();

		static CookingResult CookMesh(MeshColliderComponent& component, bool invalidateOld = false, Vector<MeshColliderData>& outData = Vector<MeshColliderData>());

		static CookingResult CookConvexMesh(const Ref<Mesh>& mesh, Vector<MeshColliderData>& outdata);
		static CookingResult CookTriangleMesh(const Ref<Mesh>& mesh, Vector<MeshColliderData>& outdata);

	private:
		// TODO: Add support for generating debug meshes (currently the Mesh class doesn't have a constructor for passing vertices and indices)
		static void GenerateDebugMesh(MeshColliderComponent& component, const MeshColliderData& colliderData);
	};


}