#pragma once
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <assimp/scene.h>

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/raw_animation.h>

#include <ozz/base/maths/math_ex.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/vec_float.h>

namespace Frost {

	inline ozz::math::Float4x4 Float4x4FromAIMatrix4x4(const aiMatrix4x4& matrix)
	{
		ozz::math::Float4x4 result;
		result.cols[0] = ozz::math::simd_float4::Load(matrix.a1, matrix.b1, matrix.c1, matrix.d1);
		result.cols[1] = ozz::math::simd_float4::Load(matrix.a2, matrix.b2, matrix.c2, matrix.d2);
		result.cols[2] = ozz::math::simd_float4::Load(matrix.a3, matrix.b3, matrix.c3, matrix.d3);
		result.cols[3] = ozz::math::simd_float4::Load(matrix.a4, matrix.b4, matrix.c4, matrix.d4);
		return result;
	}

	inline ozz::math::Float4x4 Float4x4FromGLM(const glm::mat4& matrix)
	{
		ozz::math::Float4x4 result;
		result.cols[0] = ozz::math::simd_float4::Load(matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3]);
		result.cols[1] = ozz::math::simd_float4::Load(matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3]);
		result.cols[2] = ozz::math::simd_float4::Load(matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3]);
		result.cols[3] = ozz::math::simd_float4::Load(matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]);
		return result;
	}


	class OZZImporterAssimp
	{
		OZZImporterAssimp() = delete;
		~OZZImporterAssimp() = delete;

	public:

		static bool ExtractRawSkeleton(const aiScene* scene, ozz::animation::offline::RawSkeleton& rawSkeleton, ozz::math::Float4x4& skeletonTransform)
		{
			BoneHierachy boneHierarchy(scene);
			return boneHierarchy.ExtractRawSkeleton(rawSkeleton, skeletonTransform);
		}


		static std::vector<std::string> GetAnimationNames(const aiScene* scene)
		{
			std::vector<std::string> animationNames;
			if (scene)
			{
				animationNames.reserve(scene->mNumAnimations);
				for (size_t i = 0; i < scene->mNumAnimations; ++i)
				{
					animationNames.emplace_back(scene->mAnimations[i]->mName.C_Str());
				}
			}
			return animationNames;
		}


		static bool ExtractRawAnimations(const std::string& animationName, const aiScene* scene, const ozz::animation::Skeleton& skeleton, float samplingRate, ozz::animation::offline::RawAnimation& rawAnimation)
		{
			if (!scene)
			{
				return false;
			}

			bool found = false;

			for (uint32_t animIndex = 0; animIndex < scene->mNumAnimations; ++animIndex)
			{
				aiAnimation* anim = scene->mAnimations[animIndex];
				if (anim->mName == aiString(animationName))
				{
					found = true;
					std::unordered_map<std::string, size_t> jointIndices;
					for (size_t i = 0; i < skeleton.num_joints(); ++i)
					{
						jointIndices.emplace(skeleton.joint_names()[i], i);
					}
					samplingRate = samplingRate == 0.0f ? static_cast<float>(anim->mTicksPerSecond) : samplingRate;
					if (samplingRate < 0.0001)
					{
						samplingRate = 1.0;
					}
					rawAnimation.name = animationName;
					rawAnimation.duration = static_cast<float>(anim->mDuration) / samplingRate;

					std::set<std::tuple<size_t, aiNodeAnim*>> validChannels;
					for (uint32_t channelIndex = 0; channelIndex < anim->mNumChannels; ++channelIndex)
					{
						aiNodeAnim* nodeAnim = anim->mChannels[channelIndex];
						auto it = jointIndices.find(nodeAnim->mNodeName.C_Str());
						if (it != jointIndices.end())
						{
							size_t jointIndex = it->second;
							validChannels.emplace(jointIndex, nodeAnim);
						}
					}

					
					rawAnimation.tracks.resize(skeleton.num_joints());
					for (auto [jointIndex, nodeAnim] : validChannels)
					{
						for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumPositionKeys; ++keyIndex)
						{
							aiVectorKey key = nodeAnim->mPositionKeys[keyIndex];
							rawAnimation.tracks[jointIndex].translations.emplace_back(
								static_cast<float>(key.mTime / samplingRate),
								ozz::math::Float3(
									static_cast<float>(key.mValue.x),
									static_cast<float>(key.mValue.y),
									static_cast<float>(key.mValue.z)
								)
							);
						}
						for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumRotationKeys; ++keyIndex)
						{
							aiQuatKey key = nodeAnim->mRotationKeys[keyIndex];
							rawAnimation.tracks[jointIndex].rotations.emplace_back(
								static_cast<float>(key.mTime / samplingRate),
								ozz::math::Quaternion(
									static_cast<float>(key.mValue.x),
									static_cast<float>(key.mValue.y),
									static_cast<float>(key.mValue.z),
									static_cast<float>(key.mValue.w)
								)
							);
						}
						for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumScalingKeys; ++keyIndex)
						{
							aiVectorKey key = nodeAnim->mScalingKeys[keyIndex];
							rawAnimation.tracks[jointIndex].scales.emplace_back(
								static_cast<float>(key.mTime / samplingRate),
								ozz::math::Float3(
									static_cast<float>(key.mValue.x),
									static_cast<float>(key.mValue.y),
									static_cast<float>(key.mValue.z)
								)
							);
						}
					}
					break;
				}
			}
			return found;
		}

		static void ExtractRawAnimation(const aiAnimation* animation, const ozz::animation::Skeleton& skeleton, float samplingRate, ozz::animation::offline::RawAnimation& rawAnimation)
		{
			const aiAnimation* anim = animation;
			std::unordered_map<std::string, size_t> jointIndices;

			for (size_t i = 0; i < skeleton.num_joints(); ++i)
			{
				jointIndices.emplace(skeleton.joint_names()[i], i);
			}

			samplingRate = samplingRate == 0.0f ? static_cast<float>(anim->mTicksPerSecond) : samplingRate;
			if (samplingRate < 0.0001)
			{
				samplingRate = 1.0;
			}
			rawAnimation.name = animation->mName.C_Str();
			rawAnimation.duration = static_cast<float>(anim->mDuration) / samplingRate;

			std::set<std::tuple<size_t, aiNodeAnim*>> validChannels;
			for (uint32_t channelIndex = 0; channelIndex < anim->mNumChannels; ++channelIndex)
			{
				aiNodeAnim* nodeAnim = anim->mChannels[channelIndex];
				auto it = jointIndices.find(nodeAnim->mNodeName.C_Str());
				if (it != jointIndices.end())
				{
					size_t jointIndex = it->second;
					validChannels.emplace(jointIndex, nodeAnim);
				}
			}


			rawAnimation.tracks.resize(skeleton.num_joints());
			for (auto [jointIndex, nodeAnim] : validChannels)
			{
				for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumPositionKeys; ++keyIndex)
				{
					aiVectorKey key = nodeAnim->mPositionKeys[keyIndex];
					rawAnimation.tracks[jointIndex].translations.emplace_back(
						static_cast<float>(key.mTime / samplingRate),
						ozz::math::Float3(
							static_cast<float>(key.mValue.x),
							static_cast<float>(key.mValue.y),
							static_cast<float>(key.mValue.z)
						)
					);
				}
				for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumRotationKeys; ++keyIndex)
				{
					aiQuatKey key = nodeAnim->mRotationKeys[keyIndex];
					rawAnimation.tracks[jointIndex].rotations.emplace_back(
						static_cast<float>(key.mTime / samplingRate),
						ozz::math::Quaternion(
							static_cast<float>(key.mValue.x),
							static_cast<float>(key.mValue.y),
							static_cast<float>(key.mValue.z),
							static_cast<float>(key.mValue.w)
						)
					);
				}
				for (uint32_t keyIndex = 0; keyIndex < nodeAnim->mNumScalingKeys; ++keyIndex)
				{
					aiVectorKey key = nodeAnim->mScalingKeys[keyIndex];
					rawAnimation.tracks[jointIndex].scales.emplace_back(
						static_cast<float>(key.mTime / samplingRate),
						ozz::math::Float3(
							static_cast<float>(key.mValue.x),
							static_cast<float>(key.mValue.y),
							static_cast<float>(key.mValue.z)
						)
					);
				}
			}
		}


	private:
		class BoneHierachy
		{
		public:
			BoneHierachy(const aiScene* scene) : m_Scene(scene) {}

			bool ExtractRawSkeleton(ozz::animation::offline::RawSkeleton& rawSkeleton, ozz::math::Float4x4& skeletonTransform)
			{
				if (!m_Scene)
				{
					return false;
				}

				ExtractBones();
				if (m_Bones.empty())
				{
					return false;
				}

				aiMatrix4x4 transform;
				TraverseNode(m_Scene->mRootNode, rawSkeleton, transform);
				skeletonTransform = Float4x4FromAIMatrix4x4(transform);

				return true;
			}


			void ExtractBones()
			{
				// Note: ASSIMP does not appear to support import of digital content files that contain _only_ an armature/skeleton and no mesh.
				for (uint32_t meshIndex = 0; meshIndex < m_Scene->mNumMeshes; ++meshIndex)
				{
					const aiMesh* mesh = m_Scene->mMeshes[meshIndex];
					for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
					{
						m_Bones.emplace(mesh->mBones[boneIndex]->mName.C_Str());
					}
				}
			}


			void TraverseNode(aiNode* node, ozz::animation::offline::RawSkeleton& rawSkeleton, aiMatrix4x4& transform)
			{
				bool isBone = (m_Bones.find(node->mName.C_Str()) != m_Bones.end());

				//// If node isn't a bone, but at least one of its children is, then treat the node as part of the skeleton anyway.
				//// It will end up as a "root" which then gives us a way to transform the entire skeleton
				for (uint32_t nodeIndex = 0; !isBone && nodeIndex < node->mNumChildren; ++nodeIndex)
				{
					isBone = (m_Bones.find(node->mChildren[nodeIndex]->mName.C_Str()) != m_Bones.end());
				}

				if (isBone) {
					rawSkeleton.roots.emplace_back();
					TraverseBone(node, rawSkeleton.roots.back());
				}
				else {
					for (uint32_t nodeIndex = 0; nodeIndex < node->mNumChildren; ++nodeIndex)
					{
						transform *= node->mTransformation;
						TraverseNode(node->mChildren[nodeIndex], rawSkeleton, transform);
					}
				}
			}


			void TraverseBone(aiNode* node, ozz::animation::offline::RawSkeleton::Joint& joint)
			{
				aiMatrix4x4 mat = node->mTransformation;
				aiVector3D scale;
				aiQuaternion rotation;
				aiVector3D translation;
				mat.Decompose(scale, rotation, translation);

				joint.name = node->mName.C_Str();
				joint.transform.translation = ozz::math::Float3(translation.x, translation.y, translation.z);
				joint.transform.rotation = ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
				joint.transform.scale = ozz::math::Float3(scale.x, scale.y, scale.z);

				joint.children.resize(node->mNumChildren);
				for (uint32_t nodeIndex = 0; nodeIndex < node->mNumChildren; ++nodeIndex)
				{
					TraverseBone(node->mChildren[nodeIndex], joint.children[nodeIndex]);
				}
			}


		private:
			std::set<std::string> m_Bones;
			const aiScene* m_Scene;
		};

	};
}
