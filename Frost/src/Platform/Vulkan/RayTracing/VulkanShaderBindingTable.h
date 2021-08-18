#pragma once

#include <vulkan/vulkan.hpp>

#include "Frost/Renderer/RayTracing/ShaderBindingTable.h"
#include "Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanShaderBindingTable : public ShaderBindingTable
	{
	public:
		VulkanShaderBindingTable(const Ref<Shader>& shader);
		virtual ~VulkanShaderBindingTable();

		void Destroy() override;
		
		std::tuple<Vector<VkRayTracingShaderGroupCreateInfoKHR>, Vector<VkPipelineShaderStageCreateInfo>> GetVulkanShaderInfo() const override {
			return std::make_tuple(m_ShadersInfo, m_ShaderModules);
		}

		std::array<VkStridedDeviceAddressRegionKHR, 4> GetVulkanShaderAddresses() const override;
		
	private:

		void CreateShaderBindingTableBuffer(VkPipeline pipeline);

		Vector<VkRayTracingShaderGroupCreateInfoKHR> m_ShadersInfo;
		Vector<VkPipelineShaderStageCreateInfo> m_ShaderModules;

		Ref<Shader> m_SourceShader;

		VkBuffer m_Buffer;
		VulkanMemoryInfo m_BufferMemory;

		struct ShaderStride
		{
			uint32_t RayGen = 0;
			uint32_t Hit = 0;
			uint32_t Miss = 0;
		};

		ShaderStride m_ShaderStride;

		friend class VulkanRayTracingPipeline;

	};

}