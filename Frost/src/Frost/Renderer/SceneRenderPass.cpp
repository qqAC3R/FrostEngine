#include "frostpch.h"
#include "SceneRenderPass.h"

namespace Frost
{


#if 0
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
#endif


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

	void SceneRenderPassPipeline::UpdateRendererDebugger()
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnRenderDebug();
		}
	}

	void SceneRenderPassPipeline::InitLateRenderPasses()
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->InitLate();
		}
	}

	void SceneRenderPassPipeline::ResizeRenderPasses(uint32_t width, uint32_t height)
	{
		// Resize
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnResize(width, height);
		}

		// Late resize
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnResizeLate(width, height);
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