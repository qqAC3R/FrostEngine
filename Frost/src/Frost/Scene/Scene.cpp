#include "frostpch.h"
#include "Scene.h"

#if 0
#include "Frost/Core/Application.h"

#include "Frost/Renderer/Buffers/Buffer.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Frost
{

	Scene::Scene(VkCommandPool* cmdPool)
	{
		
	}

	Scene::~Scene()
	{

	}

	void Scene::Delete()
	{
		for (auto& entity : m_Entities)
		{
			entity.second.Mesh->DeleteMesh();
		}

		if(m_Instances != NULL)
			m_Instances->Destroy();

	}

	std::vector<Ref<VulkanMesh>> Scene::GetMeshes()
	{
		std::vector<Ref<VulkanMesh>> meshes;

		for (auto& entity : m_Entities)
		{
			meshes.push_back(entity.second.Mesh);
		}

		return meshes;
	}

	std::vector<MeshInstance> Scene::GetTransformInstances()
	{
		std::vector<MeshInstance> instances;

		for (auto& entity : m_Entities)
		{
			instances.push_back(entity.second.Instance);
		}

		return instances;
	}

	Ref<Buffer> Scene::GetInstances()
	{
		std::vector<MeshInstance> instances;

		for (auto& entity : m_Entities)
		{
			instances.push_back(entity.second.Instance);
		}

		if (m_Instances != NULL)
			m_Instances->Destroy();

		//m_Instances = CreateRef<StorageBuffer>();
		//m_Instances->CreateStorageBuffer(instances.data(), instances.size() * sizeof(MeshInstance), { BufferType::AccelerationStructure });
		m_Instances = Buffer::Create(instances.data(), (uint32_t)instances.size() * sizeof(MeshInstance), { BufferType::AccelerationStructure });


		return m_Instances;

	}

	Vector<MeshInstance> Scene::GetMeshInstances()
	{
		std::vector<MeshInstance> instances;

		for (auto& entity : m_Entities)
		{
			instances.push_back(entity.second.Instance);
		}

		return instances;
	}

	std::vector<Ref<Buffer>> Scene::GetMaterialsBuffers()
	{
		std::vector<Ref<Buffer>> materials;

		for (auto& entity : m_Entities)
		{
			materials.push_back(entity.second.Mesh->GetMaterialsBuffer());
		}

		return materials;

	}

	std::vector<Ref<Buffer>> Scene::GetMatIndexBuffers()
	{
		std::vector<Ref<Buffer>> materialIndex;

		for (auto& entity : m_Entities)
		{
			materialIndex.push_back(entity.second.Mesh->GetMatIndexBuffer());
		}

		return materialIndex;

	}

	std::vector<Ref<Texture2D>> Scene::GetTextures()
	{
		std::vector<Ref<Texture2D>> textures;

		for (auto& entity : m_Entities)
		{
			for (auto& texture : entity.second.Mesh->GetTextures())
			{
				textures.push_back(texture);
			}
		}

		return textures;

	}

	std::vector<Ref<Buffer>> Scene::GetVertexBuffers()
	{
		std::vector<Ref<Buffer>> verticies;

		for (auto& entity : m_Entities)
		{
			verticies.push_back(entity.second.Mesh->GetVertexBuffer());
		}

		return verticies;

	}

	std::vector<Ref<Buffer>> Scene::GetIndexBuffers()
	{
		std::vector<Ref<Buffer>> indicies;

		for (auto& entity : m_Entities)
		{
			indicies.push_back(entity.second.Mesh->GetIndexBuffer());
		}

		return indicies;

	}

	int Scene::AddEntity(std::string filepath, const glm::mat4& position, MaterialMeshLayout material)
	{
		Ref<VulkanMesh> mesh = VulkanMesh::Create(filepath, material);

		MeshInstance instance{};
		instance.objIndex = static_cast<uint32_t>(m_Entities.size());
		instance.transform = position;
		instance.transformIT = glm::transpose(glm::inverse(instance.transform));
		instance.txtOffset = static_cast<uint32_t>(m_Entities.size());

		m_Entities[m_Index].Mesh = mesh;
		m_Entities[m_Index].Instance = instance;

		m_Index++;

		return m_Index - 1;
	}

	//void AddInstance(int id, const glm::mat4& position)
	//{
	//	MeshInstance instance{};
	//	instance.objIndex = static_cast<uint32_t>(m_Entities.size());
	//	instance.transform = position;
	//	instance.transformIT = glm::transpose(glm::inverse(instance.transform));
	//	instance.txtOffset = static_cast<uint32_t>(m_Entities.size());
	//
	//	m_Entities
	//
	//}


	void Scene::SetInstaceTransform(uint32_t id, const glm::mat4& transform)
	{
		MeshInstance instance{};
		instance.objIndex = id;
		instance.transform = transform;
		instance.transformIT = glm::transpose(glm::inverse(instance.transform));
		instance.txtOffset = id;

		m_Entities[id].Instance = instance;

		FROST_INFO("");
	}


}
#endif