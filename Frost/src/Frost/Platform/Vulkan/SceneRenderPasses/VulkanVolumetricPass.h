#pragma once

#include "Frost/Renderer/SceneRenderPass.h"
#include "Frost/Renderer/Renderer.h"

namespace Frost
{
	class VulkanVolumetricPass : public SceneRenderPass
	{
	public:
		VulkanVolumetricPass();
		virtual ~VulkanVolumetricPass();

		virtual void Init(SceneRenderPassPipeline* renderPassPipeline) override;
		virtual void InitLate() override;
		virtual void OnUpdate(const RenderQueue& renderQueue) override;
		virtual void OnRenderDebug() override;
		virtual void OnResize(uint32_t width, uint32_t height) override;
		virtual void OnResizeLate(uint32_t width, uint32_t height) override;
		virtual void ShutDown() override;

		virtual void* GetInternalData() override { return (void*)m_Data; }

		virtual const std::string& GetName() override { return m_Name; }
	private:
		// ----------- Froxel Volume Populate Pass ------------
		void FroxelVolumetricInitData(uint32_t width, uint32_t height);
		void FroxelVolumetricUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ----------- Volumetric Compute Pass ------------
		void VolumetricComputeInitData(uint32_t width, uint32_t height);
		void VolumetricComputeUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ----------- Volumetric Blur Pass ------------
		void VolumetricBlurInitData(uint32_t width, uint32_t height);
		void VolumetricBlurUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------
	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> FroxelPopulateShader;
			Ref<ComputePipeline> FroxelPopulatePipeline;
			Vector<Ref<Material>> FroxelPopulateDescriptor;
			Vector<Ref<Texture3D>> ScatExtinctionFroxelTexture;
			Vector<Ref<Texture3D>> EmissionPhaseFroxelTexture;
			Vector<Ref<BufferDevice>> FogVolumesDataBuffer;

			Ref<Shader> VolumetricComputeShader;
			Ref<ComputePipeline> VolumetricComputePipeline;
			Vector<Ref<Material>> VolumetricComputeDescriptor;
			Vector<Ref<Image2D>> VolumetricComputeTexture;


			Ref<Shader> VolumetricBlurShader;
			Ref<ComputePipeline> VolumetricBlurPipeline;
			Vector<Ref<Material>> VolumetricBlurXDescriptor;
			Vector<Ref<Material>> VolumetricBlurYDescriptor;
			Vector<Ref<Material>> VolumetricBlurUpsampleDescriptor;
			Vector<Ref<Image2D>> VolumetricBlurTexture_DirX;
			Vector<Ref<Image2D>> VolumetricBlurTexture_DirY;
			Vector<Ref<Image2D>> VolumetricBlurTexture_Upsample;


			Ref<Shader> VolumetricComputeShader;
			Ref<ComputePipeline> VolumetricComputePipeline;
			Vector<Ref<Material>> VolumetricComputeDescriptor;
			Vector<Ref<Image2D>> VolumetricComputeTexture;


		};

		struct FogVolumeParams
		{
			glm::mat4 InvTransformMatrix; // Inverse model-space transform matrix.
			glm::vec4 MieScatteringPhase; // Mie scattering (x, y, z) and phase value (w).
			glm::vec4 EmissionAbsorption; // Emission (x, y, z) and absorption (w).
		};

		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}