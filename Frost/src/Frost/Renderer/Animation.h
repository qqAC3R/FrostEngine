#pragma once

#include "Frost/Core/Timestep.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/base/containers/vector.h>
#include <ozz/animation/runtime/sampling_job.h>

#include "Frost/Renderer/Animation/AnimationBlueprint.h"

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
	class Mesh;

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
		AnimationController(Ref<Mesh> mesh);
		virtual ~AnimationController();

		Ref<AnimationBlueprint> GetAnimationBlueprint() { return m_AnimationBlueprint; }
		void SetAnimationBlueprint(Ref<AnimationBlueprint> animBlueprint) { m_AnimationBlueprint = animBlueprint; }

		const ozz::vector<ozz::math::Float4x4>& GetModelSpaceMatrices() const { return m_ModelSpaceTransforms; }

		void OnUpdate(Timestep ts);
		
		void StartAnimation() { m_AnimationPlaying = true; }
		void StopAnimation()  { m_AnimationPlaying = false; }
	private:
		struct AnimationNodeInternal
		{
			AnimationNode::NodeType Type;
			ozz::vector<ozz::math::SoaTransform> LocalSpaceTransforms; // Result matrix

			float AnimationTime = 0.0f;
		};

		struct AnimationOutputNodeInternal : public AnimationNodeInternal
		{
			Ref<AnimationNodeInternal> Result;
			ozz::vector<ozz::math::Float4x4> ResultMatrix;
		};

		struct AnimationInputNodeInternal : public AnimationNodeInternal
		{
			ozz::animation::SamplingJob::Context SamplingContext;
			InputNodeHandle VariableID;
			bool IsAnimation;
		};

		struct AnimationConditionNodeInternal : public AnimationNodeInternal
		{
			Ref<AnimationNodeInternal> ConditionBoolean;
			Ref<AnimationNodeInternal> ConditionTrue;
			Ref<AnimationNodeInternal> ConditionFalse;
		};

		struct AnimationBlendNodeInternal : public AnimationNodeInternal
		{
			Ref<AnimationNodeInternal> BlendFactorFloat;
			Ref<AnimationNodeInternal> AnimationA;
			Ref<AnimationNodeInternal> AnimationB;
		};

	private:
		struct AnimationSampler;

		// Sample current animation clip (aka "state") at current animation time
		//void SampleAnimation(AnimationSampler& animationSampler);
		//void ComputeAnimationTime(AnimationSampler& animationSampler, Timestep ts);
		//void BlendAnimations(AnimationSampler& animationFrom, AnimationSampler& animationTo, float weight);

		void BakeAnimationGraph(Ref<AnimationBlueprint> animationBluePrint);
		Ref<AnimationNodeInternal> TraverseNodes(Ref<AnimationNode> curretNode, Ref<AnimationBlueprint> animationBluePrint);
		void UpdateAnimationGraph(Timestep ts);

	private:
		//Ref<Animation> m_ActiveAnimation = nullptr;
		//Ref<Animation> m_TransitionedAnimation = nullptr;

		//ozz::vector<ozz::math::SoaTransform> m_LocalSpaceTransforms;
		//ozz::vector<ozz::math::SoaTransform> m_LocalSpaceTransforms_Transitioned;

		//Ref<Mesh> m_Mesh;

		Vector<Ref<AnimationNodeInternal>> m_BakedAnimationNodes;

		Ref<MeshSkeleton> m_MeshSkeleton;

		Vector<Ref<Animation>> m_Animations;
		Ref<AnimationBlueprint> m_AnimationBlueprint;

		ozz::vector<ozz::math::Float4x4> m_ModelSpaceTransforms; // Output transform
		ozz::vector<ozz::math::Float4x4> m_DefaultModelSpaceTransforms;
		ozz::vector<ozz::math::SoaTransform> m_DefaultTransform; // Result matrix

#if 0
		struct AnimationSampler
		{
			Ref<Animation> AnimationHandle = nullptr;
			ozz::vector<ozz::math::SoaTransform> LocalSpaceTransforms;
			ozz::animation::SamplingJob::Context SamplingContext;
			float AnimationTime;

			void Copy(AnimationSampler& sampler);
		};

		AnimationSampler m_ActiveAnimation;
		AnimationSampler m_TransitionedAnimation;
#endif

		//ozz::animation::SamplingJob::Context m_SamplingContext;

		//float m_TransitionTime = 0.0f;
		//bool m_IsTransitioning = false;


		//float m_AnimationTime = 0.0f;
		//float m_PreviousAnimationTime = -FLT_MAX;


		bool m_AnimationPlaying = true;

		friend class MeshAsset;
	};

}