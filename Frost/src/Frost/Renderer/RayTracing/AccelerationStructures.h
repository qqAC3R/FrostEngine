#pragma once

#include <glm/glm.hpp>

namespace Frost
{
	class VertexBuffer;
	class IndexBuffer;
	class Mesh;
	struct Submesh;

	struct MeshASInfo
	{
		Ref<VertexBuffer> MeshVertexBuffer;
		Ref<IndexBuffer> MeshIndexBuffer;
		Ref<IndexBuffer> SubmeshIndexBuffer;
		Vector<Submesh> SubMeshes;
		uint32_t PrimitiveCount;
		uint32_t PrimitiveOffset;

		uint32_t VertexCount;
		uint32_t VertexOffset;
	};
	
	// Only works for Vulkan/DX12 APIs
	class BottomLevelAccelerationStructure
	{
	public:
		virtual ~BottomLevelAccelerationStructure() {}
		virtual void Destroy() = 0;

		static Ref<BottomLevelAccelerationStructure> Create(const MeshASInfo& meshInfo);
	};

	// Only works for Vulkan/DX12 APIs
	class TopLevelAccelertionStructure
	{
	public:
		virtual ~TopLevelAccelertionStructure() {}
		virtual void Destroy() = 0;

		virtual void UpdateAccelerationStructure(Vector<std::pair<Ref<Mesh>, glm::mat4>>& meshes) = 0;

		static Ref<TopLevelAccelertionStructure> Create();
	};

}