#pragma once

#include <glm/glm.hpp>

struct aiNode;
struct aiAnimation;
struct aiNodeAnim;
struct aiScene;

namespace Frost
{
	class Mesh;

	class Animation
	{
	public:
		Animation(const aiAnimation* animation, Mesh* mesh);
		virtual ~Animation();

		const std::string& GetName() const { return m_Name; }

		void Reset();
		void Update(float ts);

	private:
		void ReadNodeHierarchy(float AnimationTime, const aiNode* pNode, const glm::mat4& parentTransform);

		uint32_t FindKeyFrameFromTime(float AnimationTime, const aiNodeAnim* pNodeAnim, uint32_t animType);
		glm::vec3 InterpolateTranslation(float animationTime, const aiNodeAnim* nodeAnim);
		glm::quat InterpolateRotation(float animationTime, const aiNodeAnim* nodeAnim);
		glm::vec3 InterpolateScale(float animationTime, const aiNodeAnim* nodeAnim);
	private:
		Mesh* m_InternalMesh;

		std::string m_Name;
		const aiAnimation* m_InternalAnimation;
		uint32_t m_TicksPerSecond;
		float m_Duration;

		float m_CurrentTime = 0.0f;
	};

}