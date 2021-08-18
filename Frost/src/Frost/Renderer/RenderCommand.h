#pragma once

#if 0
#include "RendererAPI.h"

namespace Frost
{

	class Renderer
	{
	public:

		inline static void Init()
		{
			s_RendererAPI->Init();
		}
		inline static void ShutDown()
		{
			s_RendererAPI->ShutDown();
		}

		inline static void BeginRenderPass(const EditorCamera& camera)
		{
			s_RendererAPI->BeginRenderPass(camera);
		}
		inline static void EndRenderPass()
		{
			s_RendererAPI->EndRenderPass();
		}


		inline static void Submit(const Ref<Mesh>& mesh, const glm::mat4& transform)
		{
			s_RendererAPI->Submit(mesh, transform);
		}
		inline static void Submit(const Ref<Mesh>& mesh, Ref<Material> material, const glm::mat4& transform)
		{
			s_RendererAPI->Submit(mesh, material, transform);
		}

	private:
		static RendererAPI* s_RendererAPI;

	};


}
#endif