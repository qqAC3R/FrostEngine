#include "frostpch.h"
#include "RendererSettings.h"

namespace Frost
{
	void RendererSettings::Initialize()
	{
		// Forward+
		ForwardPlus.UseLightHeatMap = 0;

		// Shadow Pass
		ShadowPass.CascadeSplitLambda = 0.90f;
		ShadowPass.CameraFarClip = 2000.0f;
		ShadowPass.CameraNearClip = 0.1f;
		ShadowPass.CascadeFarPlaneOffset = 0.0f;
		ShadowPass.CascadeNearPlaneOffset = 0.0f;
		ShadowPass.FadeCascades = 1;
		ShadowPass.CascadesFadeFactor = 10.0f;
		ShadowPass.m_ShowCascadesDebug = 0;
		ShadowPass.UsePCSS = 1;

		// Bloom
		Bloom.Enabled = 1;
		Bloom.Threshold = 1.0f;
		Bloom.Knee = 0.1f;

		// AO
		AmbientOcclusion.Enabled = 1;
		AmbientOcclusion.AOMode = 1; // 0 = HBAO || 1 = GTAO

		// SSR
		SSR.Enabled = 1;
		SSR.UseConeTracing = true;
		SSR.RayStepCount = 5;
		SSR.RayStepSize = 0.04f;

		// Voxel Cone Tracing
		VoxelGI.EnableVoxelization = 0;

		VoxelGI.VoxelSize = 1.0f;

		VoxelGI.UseIndirectDiffuse = 0;
		VoxelGI.UseIndirectSpecular = 0;

		VoxelGI.ConeTraceMaxDistance = 100.0f;
		VoxelGI.ConeTraceMaxSteps = 80;

		// Volumetrics
		Volumetrics.EnableVolumetrics = 1;
		Volumetrics.UseTAA = 1;
	}
}