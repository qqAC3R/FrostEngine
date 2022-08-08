#include "frostpch.h"
#include "Animation.h"

#include "Frost/Renderer/Mesh.h"

#include <glm/gtx/quaternion.hpp>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

namespace Frost
{
	Animation::Animation(const aiAnimation* animation, Mesh* mesh)
		: m_InternalAnimation(animation), m_InternalMesh(mesh), m_TicksPerSecond(animation->mTicksPerSecond), m_Duration(animation->mDuration)
	{
		m_Name = animation->mName.data;
	}

	Animation::~Animation()
	{
	}

	void Animation::Reset()
	{
		m_CurrentTime = 0.0f;
	}

	void Animation::Update(float ts)
	{
		m_CurrentTime += m_TicksPerSecond * ts;
		m_CurrentTime = fmod(m_CurrentTime, m_Duration);

		ReadNodeHierarchy(m_CurrentTime, m_InternalMesh->m_Scene->mRootNode, glm::mat4(1.0f));
	}


	namespace Utils
	{
		static glm::mat4 AssimpMat4ToGlmMat4(const aiMatrix4x4& matrix)
		{
			glm::mat4 result;
			result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
			result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
			result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
			result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
			return result;
		}

		static const aiNodeAnim* FindNodeAnim(const aiAnimation* animation, const std::string& nodeName)
		{
			for (uint32_t i = 0; i < animation->mNumChannels; i++)
			{
				const aiNodeAnim* nodeAnim = animation->mChannels[i];
				if (std::string(nodeAnim->mNodeName.data) == nodeName)
					return nodeAnim;
			}
			return nullptr;
		}
	}

	void Animation::ReadNodeHierarchy(float AnimationTime, const aiNode* pNode, const glm::mat4& parentTransform)
	{
		std::string name(pNode->mName.data);
		const aiAnimation* animation = m_InternalAnimation;
		glm::mat4 nodeTransform(Utils::AssimpMat4ToGlmMat4(pNode->mTransformation));
		const aiNodeAnim* nodeAnim = Utils::FindNodeAnim(animation, name);

		if (nodeAnim)
		{
			glm::vec3 translation = InterpolateTranslation(AnimationTime, nodeAnim);
			glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), translation);

			glm::quat rotation = InterpolateRotation(AnimationTime, nodeAnim);
			glm::mat4 rotationMatrix = glm::toMat4(rotation);

			glm::vec3 scale = InterpolateScale(AnimationTime, nodeAnim);
			glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

			nodeTransform = translationMatrix * rotationMatrix * scaleMatrix;
		}

		glm::mat4 transform = parentTransform * nodeTransform;


		if (m_InternalMesh->m_BoneMapping.find(name) != m_InternalMesh->m_BoneMapping.end())
		{
			uint32_t boneIndex = m_InternalMesh->m_BoneMapping[name];
			
			glm::mat4 inverseMatrix = glm::inverse(Utils::AssimpMat4ToGlmMat4(m_InternalMesh->m_Scene->mRootNode->mTransformation));

			m_InternalMesh->m_BoneTransforms[boneIndex] = transform * m_InternalMesh->m_BoneInfo[boneIndex].BoneOffset;
		}

		for (uint32_t i = 0; i < pNode->mNumChildren; i++)
			ReadNodeHierarchy(AnimationTime, pNode->mChildren[i], transform);
	}

	uint32_t Animation::FindKeyFrameFromTime(float AnimationTime, const aiNodeAnim* pNodeAnim, uint32_t animType)
	{
		// animType:
		//    0 - Position
		//    1 - Rotation
		//    2 - Scale

		for (uint32_t i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++)
		{
			float keyFrameTime = 0.0f;
			switch (animType)
			{
			case 0: keyFrameTime = (float)pNodeAnim->mPositionKeys[i + 1].mTime; break;
			case 1: keyFrameTime = (float)pNodeAnim->mRotationKeys[i + 1].mTime; break;
			case 2: keyFrameTime = (float)pNodeAnim->mScalingKeys[i + 1].mTime; break;
			default: FROST_ASSERT_MSG("Animation type not found!");
			}

			if (AnimationTime < keyFrameTime)
				return i;
		}
		return 0;
	}

	glm::vec3 Animation::InterpolateTranslation(float animationTime, const aiNodeAnim* nodeAnim)
	{
		if (nodeAnim->mNumPositionKeys == 1)
		{
			// No interpolation necessary for single value
			auto v = nodeAnim->mPositionKeys[0].mValue;
			return { v.x, v.y, v.z };
		}

		uint32_t PositionIndex = FindKeyFrameFromTime(animationTime, nodeAnim, 0 /* Translation */);
		uint32_t NextPositionIndex = (PositionIndex + 1);
		FROST_ASSERT_INTERNAL(NextPositionIndex > nodeAnim->mNumPositionKeys);

		float DeltaTime = (float)(nodeAnim->mPositionKeys[NextPositionIndex].mTime - nodeAnim->mPositionKeys[PositionIndex].mTime);
		float Factor = (animationTime - (float)nodeAnim->mPositionKeys[PositionIndex].mTime) / DeltaTime;

		FROST_ASSERT(Factor > 1.0f, "Translation factor must be below 1.0f");
		Factor = glm::clamp(Factor, 0.0f, 1.0f);

		const aiVector3D& Start = nodeAnim->mPositionKeys[PositionIndex].mValue;
		const aiVector3D& End = nodeAnim->mPositionKeys[NextPositionIndex].mValue;

		aiVector3D Delta = End - Start;
		auto aiVec = Start + Factor * Delta;

		return { aiVec.x, aiVec.y, aiVec.z };
	}

	glm::quat Animation::InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim)
	{
		if (nodeAnim->mNumRotationKeys == 1)
		{
			// No interpolation necessary for single value
			auto v = nodeAnim->mRotationKeys[0].mValue;
			return glm::quat(v.w, v.x, v.y, v.z);
		}

		uint32_t RotationIndex = FindKeyFrameFromTime(animationTime, nodeAnim, 1 /* Rotation*/);
		uint32_t NextRotationIndex = (RotationIndex + 1);
		FROST_ASSERT_INTERNAL(NextRotationIndex > nodeAnim->mNumRotationKeys);

		float DeltaTime = (float)(nodeAnim->mRotationKeys[NextRotationIndex].mTime - nodeAnim->mRotationKeys[RotationIndex].mTime);
		float Factor = (animationTime - (float)nodeAnim->mRotationKeys[RotationIndex].mTime) / DeltaTime;

		FROST_ASSERT(Factor > 1.0f, "Translation factor must be below 1.0f");
		Factor = glm::clamp(Factor, 0.0f, 1.0f);

		const aiQuaternion& StartRotationQ = nodeAnim->mRotationKeys[RotationIndex].mValue;
		const aiQuaternion& EndRotationQ = nodeAnim->mRotationKeys[NextRotationIndex].mValue;

		auto q = aiQuaternion();
		aiQuaternion::Interpolate(q, StartRotationQ, EndRotationQ, Factor);
		q = q.Normalize();

		return glm::quat(q.w, q.x, q.y, q.z);
	}

	glm::vec3 Animation::InterpolateScale(float animationTime, const aiNodeAnim* nodeAnim)
	{
		if (nodeAnim->mNumScalingKeys == 1)
		{
			// No interpolation necessary for single value
			auto v = nodeAnim->mScalingKeys[0].mValue;
			return { v.x, v.y, v.z };
		}

		uint32_t ScaleIndex = FindKeyFrameFromTime(animationTime, nodeAnim, 2 /* Scale */);
		uint32_t NextScaleIndex = (ScaleIndex + 1);
		FROST_ASSERT_INTERNAL(NextScaleIndex > nodeAnim->mNumScalingKeys);

		float DeltaTime = (float)(nodeAnim->mScalingKeys[NextScaleIndex].mTime - nodeAnim->mScalingKeys[ScaleIndex].mTime);
		float Factor = (animationTime - (float)nodeAnim->mScalingKeys[ScaleIndex].mTime) / DeltaTime;

		FROST_ASSERT(Factor > 1.0f, "Translation factor must be below 1.0f");
		Factor = glm::clamp(Factor, 0.0f, 1.0f);

		const auto& start = nodeAnim->mScalingKeys[ScaleIndex].mValue;
		const auto& end = nodeAnim->mScalingKeys[NextScaleIndex].mValue;

		auto delta = end - start;
		auto aiVec = start + Factor * delta;

		return { aiVec.x, aiVec.y, aiVec.z };
	}

}