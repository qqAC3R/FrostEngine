#pragma once

#if 0
#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/Mesh.h"

namespace Frost
{
	class VulkanBuffer;

	struct Entity
	{
		MeshInstance Instance;
		Ref<VulkanMesh> Mesh;
	};

	class Scene
	{
	public:
		Scene() = default;
		Scene(VkCommandPool* cmdPool);;
		virtual ~Scene();

		int AddEntity(std::string filepath, const glm::mat4& position, MaterialMeshLayout material = {});

		void Delete();

		Vector<Ref<VulkanMesh>> GetMeshes();
		Vector<MeshInstance> GetTransformInstances();
		Ref<Buffer> GetInstances();
		Vector<MeshInstance> GetMeshInstances();


		void SetInstaceTransform(uint32_t id, const glm::mat4& transform);

		Vector<Ref<Buffer>> GetVertexBuffers();
		Vector<Ref<Buffer>> GetIndexBuffers();
		Vector<Ref<Buffer>> GetMaterialsBuffers();
		Vector<Ref<Buffer>> GetMatIndexBuffers();
		Vector<Ref<Texture2D>> GetTextures();

	private:

		std::unordered_map<uint32_t, Entity> m_Entities;

		uint32_t m_Index = 0;

		Ref<Buffer> m_Instances;


	};

}
#endif