#pragma once

#include "AnimationNode.h"
#include "Frost/Asset/Asset.h"

namespace Frost
{
	class Animation;
	class MeshAsset;

	class AnimationBlueprint : public Asset
	{
	public:
		AnimationBlueprint(const MeshAsset* meshAsset);

		void AddInput(const std::string& varName, AnimationInput::Type varType);
		void AddAnimationInput(const std::string& varName, Ref<Animation> animation);
		void DeleteInput(const InputNodeHandle& inputHandle);

		void SetFloatInput(const std::string& inputName, float input);
		void SetIntInput(const std::string& inputName, int32_t input);
		void SetBoolInput(const std::string& inputName, bool input);

		Ref<AnimationNode> AddInputNode(const InputNodeHandle& inputHandle);
		Ref<AnimationNode> AddBlendNode();
		Ref<AnimationNode> AddConditionNode();

		Ref<AnimationNode> GetAnimationNodeFromPin(AnimationPinInfo* pinInfo);

		void DeleteNode(const NodeHandle& nodeHandle);

		void AddPinLink(AnimationPinInfo* inputPin, AnimationPinInfo* outputPin);
		void ErasePinLink(uint32_t index);

		static AssetType GetStaticType() { return AssetType::AnimationBlueprint; }
		virtual AssetType GetAssetType() const override { return AssetType::AnimationBlueprint; }
		virtual bool ReloadData(const std::string& filepath) override;

		bool IsAnimationGraphBaked() const { return m_IsAnimationGraphBaked; }

		const MeshAsset* GetAppropiateMeshAsset() const { return m_MeshAsset; }

		void Copy(AnimationBlueprint* animationBlueprint);
	private:
		Ref<AnimationNode> AddCustomInputNode(Ref<AnimationNode> customAnimationNode);
		Ref<AnimationNode> AddCustomOutputNode(Ref<AnimationNode> customAnimationNode);
		Ref<AnimationNode> AddCustomBlendNode(Ref<AnimationNode> customAnimationNode);
		Ref<AnimationNode> AddCustomConditionNode(Ref<AnimationNode> customAnimationNode);
		void AddPinLinkWithCustomID(LinkHandle customLinkID, AnimationPinInfo* inputPin, AnimationPinInfo* outputPin);

		void AnimationGraphNeedsToBeBaked() { m_IsAnimationGraphBaked = false; }
	private:
		HashMap<InputNodeHandle, AnimationInput> m_InputMap; 
		HashMap<NodeHandle, Ref<AnimationNode>> m_NodeMap;
		HashMap<NodeHandle, HashMap<PinHandle, Ref<AnimationPinInfo>>> m_PinMap; // Contains the pin informations from all the nodes
		Ref<AnimationNode> m_OutputNode;

		HashMap<NodeHandle, glm::vec2> m_NodePositionsMap;
		Vector<AnimationPinLink> m_PinLinks; 

		// This should restrict the user to not use blueprints from a mesh to another
		const MeshAsset* m_MeshAsset;

		bool m_IsAnimationGraphBaked = false;

		friend class AnimationNoteEditor;
		friend class AnimationController;
		friend class AnimationBlueprintSerializer;
		friend class InspectorPanel;
	};

}