#pragma once

#include <glm/glm.hpp>

namespace Frost
{
	class VertexBuffer;
	class IndexBuffer;
	class Mesh;
	
	// Only works for Vulkan/DX12 APIs
	class BottomLevelAccelerationStructure
	{
	public:
		virtual ~BottomLevelAccelerationStructure() {}

		virtual void Destroy() = 0;

		static Ref<BottomLevelAccelerationStructure> Create(const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);
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