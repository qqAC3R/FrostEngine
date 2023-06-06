#pragma once

#include "Frost/Core/Timestep.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/base/containers/vector.h>
#include <ozz/animation/runtime/sampling_job.h>

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace ozz
{

namespace animation
{
	class Skeleton;
	class Animation;
}
namespace math
{
	struct Float4x4;
	struct SoaTransform;
}

}

namespace Frost
{
	class MeshAsset;

	class MeshSkeleton
	{
	public:
		MeshSkeleton(const aiScene* scene);
		virtual ~MeshSkeleton();

		const ozz::animation::Skeleton& GetInternalSkeleton() const { return *m_Skeleton; }
		const ozz::animation::Skeleton* GetInternalSkeletonPtr() const { return m_Skeleton.get(); }
		const glm::mat3 GetSkeletonTransform() const { return m_SkeletonTransform; }
	private:
		ozz::unique_ptr<ozz::animation::Skeleton> m_Skeleton;
		glm::mat3 m_SkeletonTransform;
	};

	class Animation
	{
	public:
		Animation(const aiAnimation* animation, MeshAsset* mesh);
		virtual ~Animation();

		const std::string& GetName() const { return m_Name; }

		float GetPlayBackSpeed() const { return m_PlaybackSpeed; }
		float GetDuration() const { return m_Duration; }
		bool IsLooping() const { return m_IsLooping; }

		const ozz::animation::Animation& GetInternalAnimation() const { return *m_Animation; }
		const ozz::animation::Animation* GetInternalAnimationPtr() const { return m_Animation.get(); }
	private:
		std::string m_Name;
		ozz::unique_ptr<ozz::animation::Animation> m_Animation;
		Ref<MeshSkeleton> m_Skeleton;
		bool m_IsLoaded = false;

		float m_PlaybackSpeed = 1.0f;
		float m_Duration = 0.0f;
		bool m_IsLooping = true;

		friend class AnimationController;
	};

	class AnimationController
	{
	public:
		AnimationController();
		virtual ~AnimationController();

		void SetActiveAnimation(Ref<Animation> animation);
		Ref<Animation> GetActiveAnimation() { return m_ActiveAnimation ? m_ActiveAnimation : nullptr ; }

		float* GetAnimationTime() { return &m_AnimationTime; }
		const ozz::vector<ozz::math::Float4x4>& GetModelSpaceMatrices() const { return m_ModelSpaceTransforms; }

		void OnUpdate(Timestep ts);
		
		void StartAnimation() { m_AnimationPlaying = true; }
		void StopAnimation()  { m_AnimationPlaying = false; }

	private:
		// Sample current animation clip (aka "state") at current animation time
		void SampleAnimation();
	private:
		Ref<Animation> m_ActiveAnimation = nullptr;

		ozz::vector<ozz::math::SoaTransform> m_LocalSpaceTransforms;
		ozz::vector<ozz::math::Float4x4> m_ModelSpaceTransforms;

		ozz::animation::SamplingJob::Context m_SamplingContext;

		float m_AnimationTime = 0.0f;
		float m_PreviousAnimationTime = -FLT_MAX;

		bool m_AnimationPlaying = true;

		// TODO: TEMP
		friend class MeshAsset;
	};

}