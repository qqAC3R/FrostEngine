#pragma once

#include "Frost/Renderer/Renderer.h"

namespace Frost
{
	class VulkanRendererDebugger : public RendererDebugger
	{
	public:
		VulkanRendererDebugger();
		virtual ~VulkanRendererDebugger();

		virtual void ImGuiRender() override;

	private:

	};
}