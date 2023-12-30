#pragma once

namespace Frost
{
	class RendererSettings
	{
	private:
		void Initialize();

	public:
		struct ForwardPlusSettings
		{
			int32_t UseLightHeatMap;
		} ForwardPlus;

		struct ShadowPassSettings
		{
			float CascadeSplitLambda;
			float CameraFarClip;
			float CameraNearClip;
			float CascadeFarPlaneOffset, CascadeNearPlaneOffset;
			int32_t FadeCascades;
			float CascadesFadeFactor;
			int32_t m_ShowCascadesDebug;
			int32_t UsePCSS;
			int32_t CascadeCount;
		} ShadowPass;

		struct BloomSettings
		{
			int32_t Enabled;
			float Threshold;
			float Knee;
		} Bloom;

		struct AmbientOcclussionSettings
		{
			int32_t Enabled;
			int32_t AOMode; // 0 = HBAO || 1 = GTAO
		} AmbientOcclusion;

		struct SSRSettings
		{
			int32_t Enabled;
			int32_t UseConeTracing;
			int32_t RayStepCount;
			float RayStepSize;
			int32_t UseHizTracing;
		} SSR;

		struct VoxelGISettings
		{
			int32_t EnableGlobalIllumination;
			int32_t EnableVoxelization;

			float VoxelSize;
			int32_t UseIndirectDiffuse;
			int32_t UseIndirectSpecular;

			float ConeTraceMaxDistance;
			int32_t ConeTraceMaxSteps;
		} VoxelGI;

		struct VolumetricsSettings
		{
			int32_t EnableVolumetrics;
			int32_t UseTAA;
		} Volumetrics;

	private:
		friend class Renderer;
	};
}