#include "frostpch.h"
#include "VulkanShaderBindingTable.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanShader.h"

#include "Frost/Math/Alignment.h"

namespace Frost
{

	static VkPhysicalDeviceRayTracingPipelinePropertiesKHR s_Properties;


	namespace VulkanUtils
	{

		static VkPhysicalDeviceRayTracingPipelinePropertiesKHR GetPhysicalDeviceRayTracingProperties()
		{
			// Getting the properties needed for building the shader binding table
			vk::PhysicalDevice physicalDevice = VulkanContext::GetCurrentDevice()->GetPhysicalDevice();

			auto properties = physicalDevice.getProperties2KHR<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
			return properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
		}

		static void LoadRayTracingShader(VkShaderModule& shaderModule,                                 // reading
										 VkShaderStageFlagBits stage,								  // reading
										 Vector<VkRayTracingShaderGroupCreateInfoKHR>& shaderGroups,  // writting
										 Vector<VkPipelineShaderStageCreateInfo>& shaderStages)       // writting
		{
			switch (stage)
			{
				case VK_SHADER_STAGE_RAYGEN_BIT_KHR: break;
				case VK_SHADER_STAGE_ANY_HIT_BIT_KHR: break;
				case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR: break;
				case VK_SHADER_STAGE_MISS_BIT_KHR: break;
				case VK_SHADER_STAGE_INTERSECTION_BIT_KHR: break;
				default: FROST_ASSERT(false, "Shader type is inappropriate for ray tracing pipeline!");
			}

			VkRayTracingShaderGroupCreateInfoKHR shaderGroupInfo{};
			shaderGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;


			if (stage == VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
				shaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			else
				shaderGroupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			

			shaderGroupInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroupInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
			shaderGroupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
			shaderGroupInfo.generalShader = static_cast<uint32_t>(shaderStages.size());
			shaderGroups.push_back(shaderGroupInfo);



			VkPipelineShaderStageCreateInfo shaderStageInfo{};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.module = shaderModule;
			shaderStageInfo.flags = {};
			shaderStageInfo.pName = "main";
			shaderStageInfo.stage = stage;
			shaderStages.push_back(shaderStageInfo);
		}


		static void CreateShaderBindingTable(VkBuffer& buffer, VulkanMemoryInfo& memory, VkPipeline& pipeline, size_t shaderGroupsSize)
		{
			VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

			uint32_t groupCount = static_cast<uint32_t>(shaderGroupsSize);

			// Size of a program identifier
			uint32_t groupHandleSize = s_Properties.shaderGroupHandleSize;

			// Compute the actual size needed per SBT entry
			uint32_t groupSizeAligned = Math::AlignUp(groupHandleSize, s_Properties.shaderGroupBaseAlignment);

			// Bytes needed for the SBT.
			uint32_t sbtSize = groupCount * groupSizeAligned;


			std::vector<uint8_t> shaderHandleStorage(sbtSize);
			//shaderHandleStorage[6] = '\b';

			vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());

			VulkanBufferAllocator::CreateBuffer(sbtSize,
												{ BufferType::TransferSrc, BufferType::ShaderAddress, BufferType::ShaderBindingTable },
												VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
												buffer, memory);


			void* data;
			VulkanBufferAllocator::BindBuffer(buffer, memory, &data);
			auto* pData = reinterpret_cast<uint8_t*>(data);
			for (uint32_t g = 0; g < groupCount; g++)
			{
				memcpy(pData, shaderHandleStorage.data() + g * groupHandleSize, groupHandleSize);
				pData += groupSizeAligned;
			}
			VulkanBufferAllocator::UnbindBuffer(memory);

		}
		

	}

	VulkanShaderBindingTable::VulkanShaderBindingTable(const Ref<Shader>& shader)
		: m_SourceShader(shader)
	{
		s_Properties = VulkanUtils::GetPhysicalDeviceRayTracingProperties();

		auto vulkanShader = shader.As<VulkanShader>();
		for (auto& shaderModule : vulkanShader->m_ShaderModules)
			VulkanUtils::LoadRayTracingShader(shaderModule.second, shaderModule.first, m_ShadersInfo, m_ShaderModules);

		// Getting the number of hit/miss/gen shaders are
		for (auto& shaderSource : vulkanShader->m_ShaderSources)
		{
			switch (shaderSource.first)
			{
				case ShaderType::RayGen:	 m_ShaderStride.RayGen++; break;
				case ShaderType::ClosestHit: m_ShaderStride.Hit++;	  break;
				case ShaderType::Miss:		 m_ShaderStride.Miss++;	  break;
			}
		}

	}

	void VulkanShaderBindingTable::CreateShaderBindingTableBuffer(VkPipeline pipeline)
	{
		VulkanUtils::CreateShaderBindingTable(m_Buffer, m_BufferMemory, pipeline, m_ShadersInfo.size());
	}

	VulkanShaderBindingTable::~VulkanShaderBindingTable()
	{
	}

	void VulkanShaderBindingTable::Destroy()
	{
		VulkanBufferAllocator::DeleteBuffer(m_Buffer, m_BufferMemory);
	}

	std::array<VkStridedDeviceAddressRegionKHR, 4> VulkanShaderBindingTable::GetVulkanShaderAddresses() const
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		uint32_t groupSize = Math::AlignUp(s_Properties.shaderGroupHandleSize, s_Properties.shaderGroupBaseAlignment);
		uint32_t groupStride = groupSize;

		VkBufferDeviceAddressInfo deviceAddressInfo{};
		deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		deviceAddressInfo.buffer = m_Buffer;

		VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);


#if 0   // Example
		using Stride = VkStridedDeviceAddressRegionKHR;
		std::array<Stride, 4> strideAddresses{
			Stride{sbtAddress + 0u * groupSize, groupStride, groupSize * 1},  // raygen
			Stride{sbtAddress + 1u * groupSize, groupStride, groupSize * 1},  // miss
			Stride{sbtAddress + 2u * groupSize, groupStride, groupSize * 1},  // hit
			Stride{0u, 0u, 0u}												  // callable
		};
#endif


		auto raygen = m_ShaderStride.RayGen;
		auto hit = m_ShaderStride.Hit;
		auto miss = m_ShaderStride.Miss;

#if 1
		using Stride = VkStridedDeviceAddressRegionKHR;

		std::array<Stride, 4> strideAddresses{
			Stride{sbtAddress + 0u * groupSize,				   groupStride,  groupSize * raygen},	// raygen
			Stride{sbtAddress + raygen * groupSize,			   groupStride,  groupSize * miss},		// miss
			Stride{sbtAddress + (raygen + miss) * groupSize,   groupStride,  groupSize * hit},		// hit
			Stride{0u, 0u, 0u}																		// callable
		};
#endif



		return strideAddresses;
	}

}