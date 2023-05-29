#include "frostpch.h"
#include "Animation.h"

#include "Frost/Renderer/Mesh.h"
#include "Frost/Renderer/OZZAssimpImporter.h"

#include <glm/gtc/type_ptr.hpp>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/base/span.h>

#include <ozz/base/maths/simd_math.h>

namespace Frost
{
	static glm::mat4 Mat4FromFloat4x4(const ozz::math::Float4x4& float4x4)
	{
		glm::mat4 result;
		ozz::math::StorePtr(float4x4.cols[0], glm::value_ptr(result[0]));
		ozz::math::StorePtr(float4x4.cols[1], glm::value_ptr(result[1]));
		ozz::math::StorePtr(float4x4.cols[2], glm::value_ptr(result[2]));
		ozz::math::StorePtr(float4x4.cols[3], glm::value_ptr(result[3]));
		return result;
	}


	MeshSkeleton::MeshSkeleton(const aiScene* scene)
	{
		ozz::animation::offline::RawSkeleton rawSkeleton;
		ozz::math::Float4x4 transform;
		if (OZZImporterAssimp::ExtractRawSkeleton(scene, rawSkeleton, transform))
		{
			m_SkeletonTransform = Mat4FromFloat4x4(transform);

			ozz::animation::offline::SkeletonBuilder builder;
			m_Skeleton = builder(rawSkeleton);
			if (!m_Skeleton)
			{
				FROST_CORE_ERROR("Failed to build runtime skeleton from mesh file!");
			}
		}
		else
		{
			FROST_CORE_ERROR("No skeleton found in the mesh file!");
		}
	}

	MeshSkeleton::~MeshSkeleton()
	{
	}





	Animation::Animation(const aiAnimation* animation, Mesh* mesh)
	{
		m_Name = animation->mName.C_Str();
		m_Skeleton = mesh->m_Skeleton;
		
		const ozz::animation::Skeleton& skeleton = mesh->m_Skeleton->GetInternalSkeleton();

		ozz::animation::offline::RawAnimation rawAnimation;
		OZZImporterAssimp::ExtractRawAnimation(animation, skeleton, 0.0f, rawAnimation);

		if (rawAnimation.Validate())
		{
			ozz::animation::offline::AnimationBuilder builder;
			m_Animation = builder(rawAnimation);

			if (!m_Animation)
			{
				FROST_CORE_ERROR("Failed to build runtime animation for '{}'", animation->mName.C_Str());
				return;
			}
			else
			{
				m_IsLoaded = true;
				m_Duration = m_Animation->duration();
			}
		}
		else
		{
			FROST_CORE_ERROR("Validation failed for animation {}", animation->mName.C_Str());
			return;
		}
	}

	Animation::~Animation()
	{
	}




	AnimationController::AnimationController()
	{
	}

	AnimationController::~AnimationController()
	{
	}

	void AnimationController::SetActiveAnimation(Ref<Animation> animation)
	{
		m_ActiveAnimation = animation;
		if (m_ActiveAnimation)
		{
			m_SamplingContext.Resize(m_ActiveAnimation->m_Skeleton->GetInternalSkeleton().num_joints());
			m_LocalSpaceTransforms.resize(m_ActiveAnimation->m_Skeleton->GetInternalSkeleton().num_soa_joints());
			m_ModelSpaceTransforms.resize(m_ActiveAnimation->m_Skeleton->GetInternalSkeleton().num_joints());
		}
		else
		{
			m_SamplingContext.Resize(0);
			m_LocalSpaceTransforms.resize(0);
			m_ModelSpaceTransforms.resize(0);
		}
	}


	// TODO: deal with SkeletonTransform()
	float AngleAroundYAxis(const glm::quat& quat)
	{
		static glm::vec3 xAxis = { 1.0f, 0.0f, 0.0f };
		static glm::vec3 yAxis = { 0.0f, 1.0f, 0.0f };
		auto rotatedOrthogonal = quat * xAxis;
		auto projected = glm::normalize(rotatedOrthogonal - (yAxis * glm::dot(rotatedOrthogonal, yAxis)));
		return acos(glm::dot(xAxis, projected));
	}

	void AnimationController::OnUpdate(Timestep ts)
	{
		if (!m_ActiveAnimation) return;

		if (m_AnimationPlaying)
		{
			m_AnimationTime = m_AnimationTime + ts * m_ActiveAnimation->GetPlayBackSpeed() / m_ActiveAnimation->GetDuration();

			if (m_ActiveAnimation->IsLooping())
			{
				m_AnimationTime = m_AnimationTime - floorf(m_AnimationTime);
			}
			else
			{
				m_AnimationTime = glm::clamp(m_AnimationTime, 0.0f, 1.0f);
			}
		}


		if (m_AnimationTime != m_PreviousAnimationTime)
		{
			SampleAnimation();

			/*
			
			//if (isRootMotionEnabled)
			{
				// Need to remove root motion from the sampled bone transforms.
				// Otherwise it potentially gets applied twice (once when caller of this function "applies" the returned
				// root motion, and once again by the bone transforms).

				// Extract root transform so we can fiddle with it...
				ozz::math::SoaFloat3& soaTranslation = m_LocalSpaceTransforms[0].translation;
				ozz::math::SoaQuaternion& soaRotation = m_LocalSpaceTransforms[0].rotation;
				glm::vec3 translation = glm::vec3(ozz::math::GetX(soaTranslation.x), ozz::math::GetX(soaTranslation.y), ozz::math::GetX(soaTranslation.z));
				//// BEWARE: order of elements in glm::quat could be (w,x,y,z) or (x,y,z,w) depending on glm version
				glm::quat rotation = { ozz::math::GetX(soaRotation.w), ozz::math::GetX(soaRotation.x),ozz::math::GetX(soaRotation.y), ozz::math::GetX(soaRotation.z) };
				glm::mat4 rootTransform = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(glm::quat(rotation));

				// remove components of root transform, based on root motion extract mask
				glm::vec3 rootTranslationMask = glm::vec3(1.0f);
				float rootRotationMask = 1.0f;

				translation = rootTranslationMask * translation;

				if (rootRotationMask > 0.0f)
				{
					auto angleY = AngleAroundYAxis(rotation);
					rotation = glm::quat(cos(angleY * 0.5f), glm::vec3{ 0.0f, 1.0f, 0.0f } *sin(angleY * 0.5f));
				}
				else
				{
					rotation = glm::identity<glm::quat>();
				}
				glm::mat4 extractTransform = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(glm::quat(rotation));
				rootTransform = glm::inverse(extractTransform) * rootTransform;
				translation = rootTransform[3];
				rotation = glm::quat_cast(rootTransform);

				// put the modified root transform back into the bone transforms
				soaTranslation = ozz::math::SoaFloat3::Load(
					ozz::math::simd_float4::Load(translation.x, ozz::math::GetY(soaTranslation.x), ozz::math::GetZ(soaTranslation.x), ozz::math::GetW(soaTranslation.x)),
					ozz::math::simd_float4::Load(translation.y, ozz::math::GetY(soaTranslation.y), ozz::math::GetZ(soaTranslation.y), ozz::math::GetW(soaTranslation.y)),
					ozz::math::simd_float4::Load(translation.z, ozz::math::GetY(soaTranslation.z), ozz::math::GetZ(soaTranslation.z), ozz::math::GetW(soaTranslation.z))
				);
				soaRotation = ozz::math::SoaQuaternion::Load(
					ozz::math::simd_float4::Load(rotation.x, ozz::math::GetY(soaRotation.x), ozz::math::GetZ(soaRotation.x), ozz::math::GetW(soaRotation.x)),
					ozz::math::simd_float4::Load(rotation.y, ozz::math::GetY(soaRotation.y), ozz::math::GetZ(soaRotation.y), ozz::math::GetW(soaRotation.y)),
					ozz::math::simd_float4::Load(rotation.z, ozz::math::GetY(soaRotation.z), ozz::math::GetZ(soaRotation.z), ozz::math::GetW(soaRotation.z)),
					ozz::math::simd_float4::Load(rotation.w, ozz::math::GetY(soaRotation.w), ozz::math::GetZ(soaRotation.w), ozz::math::GetW(soaRotation.w))
				);
			}
			*/



			ozz::animation::LocalToModelJob ltm_job;
			ltm_job.skeleton = m_ActiveAnimation->m_Skeleton->GetInternalSkeletonPtr();
			ltm_job.input = ozz::make_span(m_LocalSpaceTransforms);
			ltm_job.output = ozz::make_span(m_ModelSpaceTransforms);
			if (!ltm_job.Run())
			{
				FROST_CORE_ERROR("ozz animation convertion to model space failed!");
			}

			m_PreviousAnimationTime = m_AnimationTime;
		}
	}

	void AnimationController::SampleAnimation()
	{
		ozz::animation::SamplingJob sampling_job;
		sampling_job.animation = m_ActiveAnimation->GetInternalAnimationPtr();
		sampling_job.ratio = m_AnimationTime;
		sampling_job.context = &m_SamplingContext;
		sampling_job.output = ozz::make_span(m_LocalSpaceTransforms);
		if (!sampling_job.Run())
		{
			FROST_CORE_ERROR("ozz animation sampling job failed!");
		}
	}

	// TODO: Add blending animation
	// https://github.com/guillaumeblanc/ozz-animation/blob/7fd3642f170f8ecaa31e911267e838eaf149b462/samples/additive/sample_additive.cc
}