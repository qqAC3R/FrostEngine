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





	Animation::Animation(const aiAnimation* animation, MeshAsset* mesh)
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


	AnimationController::AnimationController(Ref<Mesh> mesh)
		: m_MeshSkeleton(mesh->GetMeshAsset()->GetMeshSkeleton())
	{
		m_Animations = mesh->GetMeshAsset()->GetAnimations();
		
		// Default blueprint
		m_AnimationBlueprint = Ref<AnimationBlueprint>::Create(mesh->GetMeshAsset().Raw());
		m_AnimationBlueprint->Handle = 0;

		int32_t skeletonNumJoints = m_MeshSkeleton->GetInternalSkeleton().num_joints();
		m_ModelSpaceTransforms.resize(skeletonNumJoints);

		m_DefaultTransform.resize(skeletonNumJoints);
		for (auto& defaultTransform : m_DefaultTransform)
			defaultTransform = ozz::math::SoaTransform::identity();

		
		m_DefaultModelSpaceTransforms.resize(skeletonNumJoints);
		for (auto& defaultTransform : m_DefaultModelSpaceTransforms)
			defaultTransform = ozz::math::Float4x4::identity();
	}

	AnimationController::~AnimationController()
	{
	}

#if 0
	void AnimationController::SetActiveAnimation(Ref<Animation> animation)
	{
#if 0
		if (!animation)
			return;

		int32_t skeletonNumJoints = animation->m_Skeleton->GetInternalSkeleton().num_joints();

		if (!m_ActiveAnimation.AnimationHandle)
		{
			m_ActiveAnimation.AnimationHandle = animation;
			m_ActiveAnimation.SamplingContext.Resize(skeletonNumJoints);
			m_ActiveAnimation.LocalSpaceTransforms.resize(skeletonNumJoints);

			return;
		}
		else
		{
			m_TransitionedAnimation.AnimationHandle = animation;
		}

		if (animation)
		{
			m_TransitionTime = 0.0f;
			m_IsTransitioning = true;
			

			m_ActiveAnimation.SamplingContext.Resize(skeletonNumJoints);
			m_ActiveAnimation.LocalSpaceTransforms.resize(skeletonNumJoints);
			
			m_TransitionedAnimation.SamplingContext.Resize(skeletonNumJoints);
			m_TransitionedAnimation.LocalSpaceTransforms.resize(skeletonNumJoints);

			m_ModelSpaceTransforms.resize(skeletonNumJoints);
			m_TransitionedLocalSpaceTransforms.resize(skeletonNumJoints);
		}
		else
		{
			// TODO: Add here something maybe
			//m_SamplingContext.Resize(0);
			//m_LocalSpaceTransforms.resize(0);
			//m_ModelSpaceTransforms.resize(0);
		}
#endif
	}
#endif


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
		if (!m_AnimationPlaying) return;

		if (!m_AnimationBlueprint->IsAnimationGraphBaked())
			BakeAnimationGraph(m_AnimationBlueprint);
		
		UpdateAnimationGraph(ts);


#if 0
		if (!m_ActiveAnimation.AnimationHandle) return;

		ComputeAnimationTime(m_ActiveAnimation, ts);

		if(m_TransitionedAnimation.AnimationHandle)
			ComputeAnimationTime(m_TransitionedAnimation, ts);

#if 0
		if (m_AnimationPlaying)
		{
			m_AnimationTime = m_AnimationTime + ts * m_ActiveAnimation.AnimationHandle->GetPlayBackSpeed() / m_ActiveAnimation.AnimationHandle->GetDuration();

			if (m_ActiveAnimation.AnimationHandle->IsLooping())
			{
				m_AnimationTime = m_AnimationTime - floorf(m_AnimationTime);
			}
			else
			{
				m_AnimationTime = glm::clamp(m_AnimationTime, 0.0f, 1.0f);
			}
		}
#endif

		//FROST_CORE_INFO(m_TransitionTime);


		if (m_ActiveAnimation.AnimationTime != m_PreviousAnimationTime)
		{
			if (m_IsTransitioning)
				m_TransitionTime += ts * 4.0f;

			if (m_TransitionTime >= 1.0f)
			{
				m_IsTransitioning = false;
				m_TransitionTime = 0.0f;

				m_ActiveAnimation.Copy(m_TransitionedAnimation);
				m_TransitionedAnimation.AnimationHandle = nullptr;
			}
			else
			{

				SampleAnimation(m_ActiveAnimation);

				if (m_TransitionedAnimation.AnimationHandle)
				{
					SampleAnimation(m_TransitionedAnimation);
					BlendAnimations(m_ActiveAnimation, m_TransitionedAnimation, m_TransitionTime);
				}

				ozz::animation::LocalToModelJob ltm_job;
				ltm_job.skeleton = m_ActiveAnimation.AnimationHandle->m_Skeleton->GetInternalSkeletonPtr();
				ltm_job.output = ozz::make_span(m_ModelSpaceTransforms);
				
				if (m_TransitionedAnimation.AnimationHandle)
					ltm_job.input = ozz::make_span(m_TransitionedLocalSpaceTransforms);
				else
					ltm_job.input = ozz::make_span(m_ActiveAnimation.LocalSpaceTransforms);


				if (!ltm_job.Run())
				{
					//FROST_CORE_ERROR("ozz animation convertion to model space failed!");
				}
				else
				{
					BakeAnimationGraph(m_AnimationBlueprint);
				}
			}

			m_PreviousAnimationTime = m_ActiveAnimation.AnimationTime;

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



			
		}
#endif
	}

#if 0
	void AnimationController::SampleAnimation(AnimationSampler& animationSampler)
	{
		ozz::animation::SamplingJob samplingJob;
		samplingJob.animation = animationSampler.AnimationHandle->GetInternalAnimationPtr();
		samplingJob.ratio = animationSampler.AnimationTime;
		samplingJob.context = &animationSampler.SamplingContext;
		samplingJob.output = ozz::make_span(animationSampler.LocalSpaceTransforms);
		if (!samplingJob.Run())
		{
			FROST_CORE_ERROR("ozz animation sampling job failed!");
		}
	}


	void AnimationController::ComputeAnimationTime(AnimationSampler& animationSampler, Timestep ts)
	{
		if (m_AnimationPlaying)
		{
			animationSampler.AnimationTime = animationSampler.AnimationTime + ts * animationSampler.AnimationHandle->GetPlayBackSpeed() / animationSampler.AnimationHandle->GetDuration();

			if (animationSampler.AnimationHandle->IsLooping())
				animationSampler.AnimationTime = animationSampler.AnimationTime - floorf(animationSampler.AnimationTime);
			else
				animationSampler.AnimationTime = glm::clamp(animationSampler.AnimationTime, 0.0f, 1.0f);
		}
	}


	void AnimationController::BlendAnimations(AnimationSampler& animationFrom, AnimationSampler& animationTo, float weight)
	{
		// Prepare blending layers.
		ozz::animation::BlendingJob::Layer layers[2];
		layers[0].transform = ozz::make_span(animationFrom.LocalSpaceTransforms);
		layers[0].weight = (1.0f - weight);

		layers[1].transform = ozz::make_span(animationTo.LocalSpaceTransforms);
		layers[1].weight = (weight);


		float threshold = 1.0f;

		// Setups blending job.
		ozz::animation::BlendingJob blend_job;
		blend_job.threshold = threshold;
		blend_job.layers = layers;
		blend_job.rest_pose = animationFrom.AnimationHandle->m_Skeleton->GetInternalSkeletonPtr()->joint_rest_poses();
		blend_job.output = make_span(m_TransitionedLocalSpaceTransforms);

		if (!blend_job.Run())
		{
			FROST_CORE_ERROR("ozz animation blending job failed!");
		}
	}

#endif

	void AnimationController::BakeAnimationGraph(Ref<AnimationBlueprint> animationBluePrint)
	{
		m_BakedAnimationNodes.clear();

		Ref<AnimationNode> outputNode = animationBluePrint->m_OutputNode;
		TraverseNodes(outputNode, animationBluePrint);

		animationBluePrint->m_IsAnimationGraphBaked = true;
	}


	Ref<AnimationController::AnimationNodeInternal> AnimationController::TraverseNodes(Ref<AnimationNode> curretNode, Ref<AnimationBlueprint> animationBluePrint)
	{
		int32_t skeletonNumJoints = m_MeshSkeleton->GetInternalSkeleton().num_joints();

		switch (curretNode->Type)
		{
			case AnimationNode::NodeType::Output:
			{
				Ref<AnimationOutputNodeInternal> outputNodeInternal = Ref<AnimationOutputNodeInternal>::Create();
				outputNodeInternal->Type = AnimationNode::NodeType::Output;
				outputNodeInternal->LocalSpaceTransforms.resize(skeletonNumJoints);
				outputNodeInternal->ResultMatrix.resize(skeletonNumJoints);

				Ref<AnimationPinInfo> posePinInfo = curretNode->InputData[curretNode->Inputs["Pose"]];
				if (posePinInfo->ConnectedPin == nullptr)
				{
					outputNodeInternal->Result = nullptr;
					break;
				}
				Ref<AnimationNode> resultPoseNode = m_AnimationBlueprint->m_NodeMap[posePinInfo->ConnectedPin->AnimationParentNode];
				outputNodeInternal->Result = TraverseNodes(resultPoseNode, m_AnimationBlueprint);

				m_BakedAnimationNodes.push_back(outputNodeInternal);
				return outputNodeInternal;
				break;
			}
			case AnimationNode::NodeType::Input:
			{
				Ref<InputAnimationNode> inputNode = curretNode.As<InputAnimationNode>();
				
				Ref<AnimationInputNodeInternal> inputNodeInternal = Ref<AnimationInputNodeInternal>::Create();
				inputNodeInternal->Type = AnimationNode::NodeType::Input;
				//inputNodeInternal->LocalSpaceTransforms.resize(skeletonNumJoints);
				inputNodeInternal->VariableID = inputNode->InputHandle;
				inputNodeInternal->IsAnimation = inputNode->InputType == AnimationInput::Type::Animation;

				if (inputNodeInternal->IsAnimation)
				{
					inputNodeInternal->LocalSpaceTransforms.resize(skeletonNumJoints);
					inputNodeInternal->SamplingContext.Resize(skeletonNumJoints);
				}

				m_BakedAnimationNodes.push_back(inputNodeInternal.As<AnimationNodeInternal>());
				return inputNodeInternal.As<AnimationNodeInternal>();
				break;
			}
			case AnimationNode::NodeType::Blend:
			{
				Ref<AnimationBlendNodeInternal> blendNodeInternal = Ref<AnimationBlendNodeInternal>::Create();
				blendNodeInternal->Type = AnimationNode::NodeType::Blend;
				blendNodeInternal->LocalSpaceTransforms.resize(skeletonNumJoints);

				// BLEND FACTOR (Float)
				Ref<AnimationPinInfo> blendFactorPinInfo = curretNode->InputData[curretNode->Inputs["Blend Factor"]];
				Ref<AnimationNode> blendFactorNode = m_AnimationBlueprint->m_NodeMap[blendFactorPinInfo->ConnectedPin->AnimationParentNode];
				Ref<AnimationNodeInternal> blendFactorNodeInternal = TraverseNodes(blendFactorNode, m_AnimationBlueprint);

				// Animation A (Animation)
				Ref<AnimationPinInfo> animationA_PinInfo = curretNode->InputData[curretNode->Inputs["A"]];
				Ref<AnimationNode> animationA_Node = m_AnimationBlueprint->m_NodeMap[animationA_PinInfo->ConnectedPin->AnimationParentNode];
				Ref<AnimationNodeInternal> animationA_NodeInternal = TraverseNodes(animationA_Node, m_AnimationBlueprint);

				// Animation B (Animation)
				Ref<AnimationPinInfo> animationB_PinInfo = curretNode->InputData[curretNode->Inputs["B"]];
				Ref<AnimationNode> animationB_Node = m_AnimationBlueprint->m_NodeMap[animationB_PinInfo->ConnectedPin->AnimationParentNode];
				Ref<AnimationNodeInternal> animationB_NodeInternal = TraverseNodes(animationB_Node, m_AnimationBlueprint);

				blendNodeInternal->BlendFactorFloat = blendFactorNodeInternal;
				blendNodeInternal->AnimationA = animationA_NodeInternal;
				blendNodeInternal->AnimationB = animationB_NodeInternal;

				m_BakedAnimationNodes.push_back(blendNodeInternal.As<AnimationNodeInternal>());
				return blendNodeInternal.As<AnimationNodeInternal>();
				break;
			}
			case AnimationNode::NodeType::Condition:
			{
				Ref<AnimationConditionNodeInternal> conditionNodeInternal = Ref<AnimationConditionNodeInternal>::Create();
				conditionNodeInternal->Type = AnimationNode::NodeType::Condition;
				conditionNodeInternal->LocalSpaceTransforms.resize(skeletonNumJoints);

				// CONDITION (Boolean)
				Ref<AnimationPinInfo> conditionPinInfo = curretNode->InputData[curretNode->Inputs["Condition"]];
				Ref<AnimationNode> conditionBoolNode = m_AnimationBlueprint->m_NodeMap[conditionPinInfo->ConnectedPin->AnimationParentNode];
				Ref<AnimationNodeInternal> conditionBoolNodeInternal = TraverseNodes(conditionBoolNode, m_AnimationBlueprint);

				// Animation A (Animation)
				Ref<AnimationPinInfo> animationTrue_PinInfo = curretNode->InputData[curretNode->Inputs["True"]];
				Ref<AnimationNode> animationTrue_Node = m_AnimationBlueprint->m_NodeMap[animationTrue_PinInfo->ConnectedPin->AnimationParentNode];
				Ref<AnimationNodeInternal> animationTrue_NodeInternal = TraverseNodes(animationTrue_Node, m_AnimationBlueprint);

				// Animation B (Animation)
				Ref<AnimationPinInfo> animationFalse_PinInfo = curretNode->InputData[curretNode->Inputs["False"]];
				Ref<AnimationNode> animationFalse_Node = m_AnimationBlueprint->m_NodeMap[animationFalse_PinInfo->ConnectedPin->AnimationParentNode];
				Ref<AnimationNodeInternal> animationFalse_NodeInternal = TraverseNodes(animationFalse_Node, m_AnimationBlueprint);

				conditionNodeInternal->ConditionBoolean = conditionBoolNodeInternal;
				conditionNodeInternal->ConditionTrue = animationTrue_NodeInternal;
				conditionNodeInternal->ConditionFalse = animationFalse_NodeInternal;

				m_BakedAnimationNodes.push_back(conditionNodeInternal.As<AnimationNodeInternal>());
				return conditionNodeInternal.As<AnimationNodeInternal>();
				break;
			}
		}
	}


	static void ComputeAnimationTime_New(Timestep ts, float& animationTime, Ref<Animation> animation)
	{
		animationTime = animationTime + ts * animation->GetPlayBackSpeed() / animation->GetDuration();

		if (animation->IsLooping())
			animationTime = animationTime - floorf(animationTime);
		else
			animationTime = glm::clamp(animationTime, 0.0f, 1.0f);
	}

	void AnimationController::UpdateAnimationGraph(Timestep ts)
	{
		for (auto& animationNode : m_BakedAnimationNodes)
		{
			switch (animationNode->Type)
			{
				case AnimationNode::NodeType::Input:
				{
					Ref<AnimationInputNodeInternal> inputNode = animationNode.As<AnimationInputNodeInternal>();
					if (inputNode->IsAnimation)
					{
						Ref<Animation> animation = m_AnimationBlueprint->m_InputMap[inputNode->VariableID].Data.As<Animation>();

						ComputeAnimationTime_New(ts, inputNode->AnimationTime, animation);

						ozz::animation::SamplingJob samplingJob;
						samplingJob.animation = animation->GetInternalAnimationPtr();
						samplingJob.ratio = inputNode->AnimationTime;
						samplingJob.context = &inputNode->SamplingContext;
						samplingJob.output = ozz::make_span(inputNode->LocalSpaceTransforms);
						if (!samplingJob.Run())
						{
							FROST_CORE_ERROR("ozz animation sampling job failed!");
						}
					}
					break;
				}

				case AnimationNode::NodeType::Condition:
				{
					Ref<AnimationConditionNodeInternal> conditionNode = animationNode.As<AnimationConditionNodeInternal>();

					Ref<AnimationInputNodeInternal> conditionBoolNode = conditionNode->ConditionBoolean.As<AnimationInputNodeInternal>();
					bool condition = *(m_AnimationBlueprint->m_InputMap[conditionBoolNode->VariableID].Data.As<bool>().Raw());

					if (condition)
					{
						Ref<AnimationNodeInternal> animationTrueNode = conditionNode->ConditionTrue;
						conditionNode->LocalSpaceTransforms = animationTrueNode->LocalSpaceTransforms;
						conditionNode->AnimationTime = animationTrueNode->AnimationTime;
					}
					else
					{
						Ref<AnimationNodeInternal> animationFalseNode = conditionNode->ConditionFalse;
						conditionNode->LocalSpaceTransforms = animationFalseNode->LocalSpaceTransforms;
						conditionNode->AnimationTime = animationFalseNode->AnimationTime;
					}
					break;
				}

				case AnimationNode::NodeType::Blend:
				{
					Ref<AnimationBlendNodeInternal> blendNode = animationNode.As<AnimationBlendNodeInternal>();

					Ref<AnimationInputNodeInternal> blendFactorNode = blendNode->BlendFactorFloat.As<AnimationInputNodeInternal>();
					float blendFactorFloat = *(m_AnimationBlueprint->m_InputMap[blendFactorNode->VariableID].Data.As<float>().Raw());

					Ref<AnimationNodeInternal> animationA_Node = blendNode->AnimationA.As<AnimationNodeInternal>();
					Ref<AnimationNodeInternal> animationB_Node = blendNode->AnimationB.As<AnimationNodeInternal>();


					// Prepare blending layers.
					ozz::animation::BlendingJob::Layer layers[2];
					layers[0].transform = ozz::make_span(animationA_Node->LocalSpaceTransforms);
					layers[0].weight = (blendFactorFloat);

					layers[1].transform = ozz::make_span(animationB_Node->LocalSpaceTransforms);
					layers[1].weight = (1.0f - blendFactorFloat);

					float threshold = 1.0f;

					// Setups blending job.
					ozz::animation::BlendingJob blendJob;
					blendJob.threshold = threshold;
					blendJob.layers = layers;
					blendJob.rest_pose = m_MeshSkeleton->GetInternalSkeletonPtr()->joint_rest_poses();
					blendJob.output = make_span(blendNode->LocalSpaceTransforms);

					if (!blendJob.Run())
					{
						FROST_CORE_ERROR("ozz animation blending job failed!");
					}

					break;
				}
				

				case AnimationNode::NodeType::Output:
				{
					Ref<AnimationOutputNodeInternal> outputNode = animationNode.As<AnimationOutputNodeInternal>();

					outputNode->LocalSpaceTransforms;

					ozz::animation::LocalToModelJob localToModelJob;
					localToModelJob.skeleton = m_MeshSkeleton->GetInternalSkeletonPtr();
					localToModelJob.output = ozz::make_span(outputNode->ResultMatrix);

					if(outputNode->Result)
						localToModelJob.input = ozz::make_span(outputNode->Result->LocalSpaceTransforms);
					else
					{
						localToModelJob.input = ozz::make_span(m_DefaultTransform);
						m_ModelSpaceTransforms = m_DefaultModelSpaceTransforms;
						break;
					}
					
					if (!localToModelJob.Run())
					{
						FROST_CORE_ERROR("ozz animation convertion to model space failed!");
					}

					m_ModelSpaceTransforms = outputNode->ResultMatrix;

					break;
				}
			}
		}

	}
}