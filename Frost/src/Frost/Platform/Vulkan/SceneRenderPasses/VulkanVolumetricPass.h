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

		// -------------- Volumetric Pass ----------------
		void VolumetricsInitData(uint32_t width, uint32_t height);
		void VolumetricsUpdate(const RenderQueue& renderQueue);
		// -----------------------------------------------

		// -------------- Gaussian Pass ----------------
		void GaussianBlurInitData(uint32_t width, uint32_t height);
		void GaussianBlurUpdate(const RenderQueue& renderQueue);
		// -----------------------------------------------

		// ----------- Froxel Volume Populate Pass ------------
		void FroxelVolumetricInitData(uint32_t width, uint32_t height);
		void FroxelVolumetricUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ----------- Volumetric Compute Pass ------------
		void VolumetricComputeInitData(uint32_t width, uint32_t height);
		void VolumetricComputeUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------
	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> VolumetricsShader;
			Ref<ComputePipeline> VolumetricsPipeline;
			Vector<Ref<Material>> VolumetricsDescriptor;
			Vector<Ref<Image2D>> VolumetricsTexture;

			Ref<Shader> GaussianBlurShader;
			Ref<ComputePipeline> GaussianBlurPipeline;
			Vector<Ref<Material>> GaussianBlurDescriptor;
			Vector<Ref<Image2D>> GaussianBlurTexture;

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