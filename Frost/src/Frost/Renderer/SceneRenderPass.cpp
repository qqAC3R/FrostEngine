#include "frostpch.h"
#include "SceneRenderPass.h"

namespace Frost
{

	template <class T>
	Ref<T> SceneRenderPassPipeline::GetRenderPass()
	{
		// Getting the id of the renderPass (by template)
		auto typeIdx = std::type_index(typeid(T));


		// Checking if the id of the renderPass is found
		auto it = m_RenderPassesByTypeId.find(typeIdx);

		if (it == m_RenderPassesByTypeId.end())
		{
			FROST_WARN("Error: The requested of RenderPass {0} does not exist.", typeIdx);
			return nullptr;
		}

		// Return the renderPass as the type from the template
		return it->second.As<T>();
	}

	template <class T>
	Ref<T> SceneRenderPassPipeline::GetRenderPass(const std::string& name)
	{
		// Checking if the name of the renderPass is found
		auto it = m_RenderPassesMap.find(name);

		if (it == m_RenderPassesMap.end())
		{
			FROST_WARN("Error: The requested of RenderPass {0} does not exist.", name);
			return nullptr;
		}

		// Return the renderPass as the type from the template
		return it->second.As<T>();
	}

	Ref<SceneRenderPass> SceneRenderPassPipeline::GetRenderPass(const std::string& name)
	{
		return GetRenderPass<SceneRenderPass>(name);
	}


	void SceneRenderPassPipeline::UpdateRenderPasses(const RenderQueue& renderQueue)
	{
		if (renderQueue.m_Data.size() != 0)
		{
			for (auto& renderPass : m_RenderPasses)
			{
				renderPass->OnUpdate(renderQueue);
			}
		}
	}

	void SceneRenderPassPipeline::ResizeRenderPasses(uint32_t width, uint32_t height)
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnResize(width, height);
		}
	}

	void SceneRenderPassPipeline::ShutDown()
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->ShutDown();
		}
	}


}