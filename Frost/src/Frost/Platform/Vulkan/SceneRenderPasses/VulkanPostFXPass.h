#pragma once

#include "Frost/Renderer/SceneRenderPass.h"

typedef struct VkSampler_T* VkSampler;

namespace Frost
{
	class VulkanPostFXPass : public SceneRenderPass
	{
	public:
		VulkanPostFXPass();
		virtual ~VulkanPostFXPass();

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

		// -------------- Hierarchal Z Buffer --------------------
		void HZBInitData(uint32_t width, uint32_t height);
		void HZBUpdate(const RenderQueue& renderQueue);
		// --------------------------------------------------------


		// ---------------------- SSR ----------------------------
		// SSR + Cone Tracing
		void SSRInitData(uint32_t width, uint32_t height);
		void SSRUpdate(const RenderQueue& renderQueue);

		// SSR Gaussian Blur
		void SSRFilterInitData(uint32_t width, uint32_t height);
		void SSRFilterUpdate(const RenderQueue& renderQueue);

		// Visibility Buffer for the SSR
		void VisibilityInitData(uint32_t width, uint32_t height); // Deprecated
		void VisibilityUpdate(const RenderQueue& renderQueue);    // Deprecated
		// --------------------------------------------------------


		// ----------------- Ambient Occlusion --------------------
		void AmbientOcclusionInitData(uint32_t width, uint32_t height);
		void AmbientOcclusionUpdate(const RenderQueue& renderQueue);

		void SpatialDenoiserInitData(uint32_t width, uint32_t height);
		void SpatialDenoiserUpdate(const RenderQueue& renderQueue);

		void AmbientOcclusionTAAInitData(uint32_t width, uint32_t height);
		void AmbientOcclusionTAAUpdate(const RenderQueue& renderQueue);
		// --------------------------------------------------------


		// ------------------- Bloom ------------------------------
		void BloomInitData(uint32_t width, uint32_t height);
		void BloomUpdate(const RenderQueue& renderQueue);
		// --------------------------------------------------------

		// ------------------- FXAA ------------------------------
		void FXAAInitData(uint32_t width, uint32_t height);
		void FXAAUpdate(const RenderQueue& renderQueue);
		// --------------------------------------------------------

		// ----------- Temporal Anti-Aliasing ---------------------
		void TAAInitData(uint32_t width, uint32_t height);
		void TAAUpdate(const RenderQueue& renderQueue);
		// --------------------------------------------------------


#if 1
		// ------------------- Bloom Convolution ------------------------------
		void BloomConvolutionInitData(uint32_t width, uint32_t height, bool initalizeData);
		void BloomConvolutionComputeKernel(const std::string& kernelNewFilepath, uint32_t width, uint32_t height);
		void BloomConvolutionUpdate(const RenderQueue& renderQueue);

		void LoadBloomDirtImage(const std::string& filepath);
		// --------------------------------------------------------

		// ---------------- Bloom Convolution Filter  -------------------------
		void BloomConvolutionFilterInitData(uint32_t width, uint32_t height);
		void BloomConvolutionFilterUpdate(const RenderQueue& renderQueue);
		// --------------------------------------------------------
#endif


		// ---------------- Hillaire 2020 -------------------------
		void ApplyAerialInitData(uint32_t width, uint32_t height); // Deprecated
		void ApplyAerialUpdate(const RenderQueue& renderQueue);	   // Deprecated
		// --------------------------------------------------------


		// ------------------ Composite ---------------------------
		void ColorCorrectionInitData(uint32_t width, uint32_t height);
		void ColorCorrectionUpdate(const RenderQueue& renderQueue, uint32_t target);
		// --------------------------------------------------------

		void CalculateMipLevels(uint32_t width, uint32_t height);

		void UpdateRenderingSettings();

	private:
		SceneRenderPassPipeline* m_RenderPassPipeline;

		struct InternalData
		{
			// General
			uint32_t ScreenMipLevel;

			// SSR
			Ref<Shader> SSRShader;
			Ref<ComputePipeline> SSRPipeline;
			Vector<Ref<Material>> SSRDescriptor;
			Vector<Ref<Image2D>> SSRTexture;

			// Pre-filtered SSR color buffer
			Vector<Ref<Image2D>> BlurredColorBuffer;
			Ref<Shader> BlurShader;
			Ref<ComputePipeline> BlurPipeline;
			Vector<Vector<Ref<Material>>> BlurShaderDescriptor;

			// Depth pyramid construction
			Ref<Shader> HZBShader; // Hi-Z Buffer Builder Compute Shader
			Ref<ComputePipeline> HZBPipeline;
			Vector<Vector<Ref<Material>>> HZBDescriptor;
			Vector<Ref<Image2D>> DepthPyramid;
			Vector<VkSampler> HZBNearestSampler; // For Occlusion Culling
			Vector<VkSampler> HZBMinReductionSampler; // For Occlusion Culling
			Vector<VkSampler> HZBMaxReductionSampler; // For Occlusion Culling

			// Visibility buffer
			Ref<Shader> VisibilityShader;
			Ref<ComputePipeline> VisibilityPipeline;
			Vector<Vector<Ref<Material>>> VisibilityDescriptor;
			Vector<Ref<Image2D>> VisibilityImage;

			// GTAO pass
			Ref<Shader> AO_Shader;
			Ref<ComputePipeline> AO_Pipeline;
			Vector<Ref<Material>> AO_Descriptor;
			Vector<Ref<Image2D>> AO_Image;

			// Denoising pass
			Ref<Shader> DenoiserShader;
			Ref<ComputePipeline> DenoiserPipeline;
			Vector<Ref<Material>> DenoiserDescriptor;
			Vector<Ref<Image2D>> DenoiserImage;
			Vector<Ref<Material>> DenoiserUpsampledDescriptor;
			Vector<Ref<Image2D>> DenoiserUpsampledImage;

			// Ambient occlusion accumulation
			Ref<Shader> AmbientOcclusionTAAShader;
			Ref<ComputePipeline> AmbientOcclusionTAAPipeline;
			Vector<Ref<Material>> AmbientOcclusionTAADescriptor;
			Vector<Ref<Image2D>> AmbientOcclusionTAAImage;

			// Bloom pass
			Ref<Shader> BloomShader;
			Ref<ComputePipeline> BloomPipeline;
			Vector<Ref<Material>> BloomDescriptor;
			Vector<Ref<Image2D>> Bloom_DownsampledTexture;
			Vector<Ref<Image2D>> Bloom_UpsampledTexture;

#if 1
			// Bloom Convolution pass
			using ConvolutionSize = uint32_t;
			HashMap<ConvolutionSize, Ref<Shader>> BloomConvolutionShader;
			HashMap<ConvolutionSize, Ref<ComputePipeline>> BloomConvPipeline;
			Vector<Ref<Material>> BloomConvDescriptor;
			Vector<Ref<Image2D>> BloomConvTexture;

			Ref<Shader> BloomExandImageShader;
			Ref<ComputePipeline> BloomExandImagePipeline;
			Vector<Ref<Material>> BloomExandImageDescriptor;
			Vector<Ref<Image2D>> BloomExandImage;

			Ref<Material> BloomExandKernelDescriptor;
			Ref<Material> BloomConvKernelDescriptor;
			Ref<Texture2D> BloomDirtImage; // Imported kernel texture
			Ref<Texture2D> BloomKernelImage; // Imported kernel texture
			Ref<Image2D> BloomKernelExpandImage; // Kernet texture decomposed to frequency domain
			Ref<Image2D> BloomKernelFFTImage; // Kernet texture decomposed to frequency domain


			Ref<Shader> BloomKernelConvertShader; // Convert the Bloom Kernel from RGBA32F format to RGBA8 to be able to show it to the user
			Ref<ComputePipeline> BloomKernelConvertPipeline;
			Ref<Material> BloomKernelConvertDescriptor;
			Ref<Image2D> BloomKernelImGuiImage; // Kernet texture decomposed to frequency domain
#endif


			// Apply Aerial Perspective + Exponential fog
			Ref<Shader> ApplyAerialShader;
			Ref<ComputePipeline> ApplyAerialPipeline;
			Vector<Ref<Material>> ApplyAerialDescriptor;
			Vector<Ref<Image2D>> ApplyAerialImage;

			// Color correction
			Ref<Shader> ColorCorrectionShader;
			Ref<ComputePipeline> ColorCorrectionPipeline;
			Vector<Ref<Material>> ColorCorrectionDescriptor;
			Vector<Ref<Image2D>> ColorCorrectionTexture;
			Vector<Ref<Image2D>> FinalTexture;

			// FXAA
			Ref<Shader> FXAAShader;
			Ref<ComputePipeline> FXAAPipeline;
			Vector<Ref<Material>> FXAADescriptor;
			Vector<Ref<Image2D>> FXAAImage;

			// TAA
			Ref<Shader> TAAShader;
			Ref<ComputePipeline> TAAPipeline;
			Vector<Ref<Material>> TAADescriptor;
			Vector<Ref<Image2D>> TAATextureAcummulation;
		};

		InternalData* m_Data;
		std::string m_Name;


#if 0
		struct BloomSettings
		{
			bool Enabled = true;
			float Threshold = 1.0f;
			float Knee = 0.1f;
		} m_BloomSettings;

		struct AmbientOcclussionSettings
		{
			int32_t AOMode = 1; // 0 = HBAO || 1 = GTAO
		} m_AOSettings;

		struct SSRSettings
		{
			int32_t UseConeTracing = true;
			int32_t RayStepCount = 5;
			float RayStepSize = 0.04f;
		} m_SSRSettings;
#endif

		struct CompositePassSettings {
			float Gamma;
			float Exposure;
			float Stage;

			int32_t UseSSR = 1;
			int32_t UseAO = 1;
			int32_t UseBloom = 1;
		} m_CompositeSetings;


		friend class SceneRenderPassPipeline;
	};
}