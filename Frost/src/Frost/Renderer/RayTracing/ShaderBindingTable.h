#pragma once

#include "Frost/Renderer/Shader.h"

/* Vulkan structs */
struct VkRayTracingShaderGroupCreateInfoKHR;
struct VkPipelineShaderStageCreateInfo;
struct VkStridedDeviceAddressRegionKHR;

namespace Frost
{


	// NOTE: Only available for Vulkan/DX12
	class ShaderBindingTable
	{
	public:
		virtual ~ShaderBindingTable() {}

		virtual void Destroy() = 0;

		/* Vulkan specific */
		virtual std::tuple<Vector<VkRayTracingShaderGroupCreateInfoKHR>, std::vector<VkPipelineShaderStageCreateInfo>> GetVulkanShaderInfo() const = 0;
		virtual std::array<VkStridedDeviceAddressRegionKHR, 4> GetVulkanShaderAddresses() const = 0;


		static Ref<ShaderBindingTable> Create(const Ref<Shader>& shader);

	};

}