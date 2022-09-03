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
		void FroxelPopulateInitData(uint32_t width, uint32_t height);
		void FroxelPopulateUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ----------- Froxel Light Injection Pass ------------
		void FroxelLightInjectInitData(uint32_t width, uint32_t height);
		void FroxelLightInjectUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ----------- Froxel Temporal Anti Aliasing ----------
		void FroxelTAAInitData(uint32_t width, uint32_t height);
		void FroxelTAAUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ------------ Froxel Final Gather Pass --------------
		void FroxelFinalComputeInitData(uint32_t width, uint32_t height);
		void FroxelFinalComputeUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------


		// ----------- Volumetric Compute Pass ------------
		void VolumetricComputeInitData(uint32_t width, uint32_t height);
		void VolumetricComputeUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

		// ----------- Volumetric Blur Pass ------------
		void VolumetricBlurInitData(uint32_t width, uint32_t height);
		void VolumetricBlurUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------


		// ----------- Cloud Noise Compute ------------
		void CloudNoiseCompute(uint32_t width, uint32_t height);
		
		void CloudComputeInitData(uint32_t width, uint32_t height);
		void CloudComputeUpdate(const RenderQueue& renderQueue);
		// ----------------------------------------------------

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			Ref<Shader> FroxelPopulateShader;
			Ref<ComputePipeline> FroxelPopulatePipeline;
			Vector<Ref<Material>> FroxelPopulateDescriptor;
			Vector<Ref<Texture3D>> ScatExtinctionFroxelTexture;
			Vector<Ref<Texture3D>> EmissionPhaseFroxelTexture; // Also used as the final light gather texture later
			Vector<Ref<BufferDevice>> FogVolumesDataBuffer;

			Ref<Shader> FroxelLightInjectShader;
			Ref<ComputePipeline> FroxelLightInjectPipeline;
			Vector<Ref<Material>> FroxelLightInjectDescriptor;

			Ref<Shader> FroxelFinalComputeShader;
			Ref<ComputePipeline> FroxelFinalComputePipeline;
			Vector<Ref<Material>> FroxelFinalComputeDescriptor;
			
			Ref<Shader> FroxelTAAShader;
			Ref<ComputePipeline> FroxelTAAPipeline;
			Vector<Ref<Material>> FroxelTAADescriptor;
			Vector<Ref<Texture3D>> FroxelResolveTAATexture;



			Ref<Shader> VolumetricComputeShader;
			Ref<ComputePipeline> VolumetricComputePipeline;
			Vector<Ref<Material>> VolumetricComputeDescriptor;
			Vector<Ref<Image2D>> VolumetricComputeTexture;


			Ref<Shader> VolumetricBlurShader;
			Ref<ComputePipeline> VolumetricBlurPipeline; // Used for clouds too
			Vector<Ref<Material>> VolumetricBlurXDescriptor;
			Vector<Ref<Material>> VolumetricBlurYDescriptor;
			Vector<Ref<Image2D>> VolumetricBlurTexture_DirX; // Direction X blur
			Vector<Ref<Image2D>> VolumetricBlurTexture_DirY; // Direction Y blur


			Ref<Shader> WoorleyNoiseShader;
			Ref<ComputePipeline> WoorleyNoisePipeline;
			Ref<Material> WoorleyNoiseDescriptor;
			Ref<Texture3D> ErroderNoiseTexture;

			Ref<Shader> PerlinNoiseShader;
			Ref<ComputePipeline> PerlinNoisePipeline;
			Ref<Material> PerlinNoiseDescriptor;
			Ref<Texture3D> CloudNoiseTexture; // Woorley + Perlin


			Ref<Shader> CloudComputeShader;
			Ref<ComputePipeline> CloudComputePipeline;
			Vector<Ref<Material>> CloudComputeDescriptor;
			Vector<Ref<Image2D>> CloudComputeTexture;
			Vector<Ref<BufferDevice>> CloudVolumesDataBuffer;

			Vector<Ref<Image2D>> CloudComputeBlurTexture_DirX; // Direction X blur
			Vector<Ref<Image2D>> CloudComputeBlurTexture_DirY; // Direction Y blur
			Vector<Ref<Material>> CloudComputeBlurXDescriptor;
			Vector<Ref<Material>> CloudComputeBlurYDescriptor;

			glm::mat4 CustomProjectionMatrix;
			float CameraFOV;
			int32_t m_UseTAA = 1;
			int32_t m_UseVolumetrics = 1;
		};

		
		InternalData* m_Data;
		std::string m_Name;

		friend class SceneRenderPassPipeline;
	};
}