#pragma once

#include "Frost/Platform/Vulkan/Vulkan.h"

#include "Frost/Renderer/RayTracing/ShaderBindingTable.h"
#include "Frost/Platform/Vulkan/Buffers/VulkanBufferAllocator.h"

namespace Frost
{
	struct VulkanMemoryInfo;

	class VulkanShaderBindingTable : public ShaderBindingTable
	{
	public:
		VulkanShaderBindingTable(const Ref<Shader>& shader);
		virtual ~VulkanShaderBindingTable();

		void Destroy() override;
		
		std::array<VkStridedDeviceAddressRegionKHR, 4> GetVulkanShaderAddresses() const;
		std::tuple<Vector<VkRayTracingShaderGroupCreateInfoKHR>, Vector<VkPipelineShaderStageCreateInfo>> GetVulkanShaderInfo() const
		{
			return std::make_tuple(m_ShadersInfo, m_ShaderModules);
		}
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