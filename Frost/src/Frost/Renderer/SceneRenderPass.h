#pragma once

#include "Frost/Renderer/RendererAPI.h"

#include <typeindex>

namespace Frost
{
	class SceneRenderPassPipeline;

	class SceneRenderPass
	{
	public:
		virtual ~SceneRenderPass() {}

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) = 0;
		virtual void InitLate() = 0; // After initializating all the renderpasses, `init late` is being called
		virtual void OnUpdate(const RenderQueue& renderQueue) = 0;
		virtual void OnResize(uint32_t width, uint32_t height) = 0;
		virtual void OnResizeLate(uint32_t width, uint32_t height) = 0;  // After resizing all the renderpasses, `resize late` is being called
		virtual void ShutDown() = 0;

		virtual void* GetInternalData() = 0;

		virtual const std::string& GetName() = 0;
	};

	class SceneRenderPassPipeline
	{
	public:
		SceneRenderPassPipeline() = default;
		virtual ~SceneRenderPassPipeline() {}
		void ShutDown();

		template <typename T>
		void AddRenderPass(Ref<T> renderPass)
		{
			if (!std::is_base_of<SceneRenderPass, T>::value)
				FROST_CORE_ERROR("The SceneRenderPass needs to derive the main class");

			// Setting up the id of the renderPass (by template)
			std::type_index templateID = std::type_index(typeid(T));
			auto sceneRenderPass = renderPass.As<SceneRenderPass>();
			sceneRenderPass->Init(this);

			m_RenderPasses.push_back(sceneRenderPass);
			m_RenderPassesByTypeId[templateID] = sceneRenderPass;
			m_RenderPassesMap[sceneRenderPass->GetName()] = sceneRenderPass;
		}

		template <class T, class... Ts>
		void AddRenderPass(Ref<T> renderPass, Ref<Ts>... renderPasses)
		{
			AddRenderPass(renderPass);
			AddRenderPass(renderPasses...);
		}

		void ResizeRenderPasses(uint32_t width, uint32_t height);

		template <class T>
		Ref<T> GetRenderPass();

		template <class T>
		Ref<T> GetRenderPass(const std::string& name);
		Ref<SceneRenderPass> GetRenderPass(const std::string& name);

		template <typename T>
		typename T::InternalData* GetRenderPassData()
		{
			if(!std::is_base_of<SceneRenderPass, T>::value)
				FROST_CORE_ERROR("The SceneRenderPass needs to derive the main class");

			auto typeIdx = std::type_index(typeid(T));
			if (m_RenderPassesByTypeId.find(typeIdx) == m_RenderPassesByTypeId.end())
				FROST_ASSERT(false, "Couldn't find the renderpass");

			return static_cast<typename T::InternalData*>(m_RenderPassesByTypeId[typeIdx]->GetInternalData());
		}
		
		void UpdateRenderPasses(const RenderQueue& renderQueue);
		void InitLateRenderPasses();

	private:
		Vector<Ref<SceneRenderPass>> m_RenderPasses;

		std::unordered_map<std::string, Ref<SceneRenderPass>> m_RenderPassesMap;
		std::unordered_map<std::type_index, Ref<SceneRenderPass>> m_RenderPassesByTypeId;
	};


}